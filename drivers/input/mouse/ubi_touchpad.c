
#include "asm/stat.h"
#include "linux/input-event-codes.h"
#include "linux/jiffies.h"
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#define MIN_X_COORDINATE 0
#define MAX_X_COORDINATE 220
#define MIN_Y_COORDINATE 0
#define MAX_Y_COORDINATE 120

struct i2c_ts_priv {
  struct i2c_client *client;
  struct input_dev *input;
  struct delayed_work work;
  int irq;
};

static int i2c_tp_open(struct input_dev *dev) { return 0; }

static void i2c_tp_close(struct input_dev *dev) {}

static int lastx = 0, lasty = 0, state = 0;
static u64 s1Time = 0;
static void i2c_tp_poscheck(struct work_struct *work) {
  struct i2c_ts_priv *priv = container_of(work, struct i2c_ts_priv, work.work);

  char buf[2];
  // int number;
  int xpos, ypos, dx = 0, dy = 0;

  memset(buf, 0, sizeof(buf));

  if (i2c_master_recv(priv->client, buf, 2) != 2) {
    dev_err(&priv->client->dev, "Unable to read i2c data\n");
    goto out;
  }
  xpos = buf[0];
  ypos = buf[1];

  if ((xpos != 0) && (ypos != 0)) { // touch

    dx = xpos - lastx;
    dy = ypos - lasty;
    lastx = xpos;
    lasty = ypos;

    switch (state) {
    case 0:
      state = 1;
      s1Time = get_jiffies_64();
      goto touchFin;
    case 2:
      if ((get_jiffies_64() - s1Time) < msecs_to_jiffies(200)) {
        input_report_key(priv->input, BTN_LEFT, 1);
        input_sync(priv->input);
        state = 3;
      }else{
        state = 0;
      }
      goto touchFin;
    }

    input_report_rel(priv->input, REL_X, dx);
    input_report_rel(priv->input, REL_Y, dy);
    input_sync(priv->input);

  touchFin:
    ;
  } else { // Released
    switch (state) {
    case 1:
      if ((get_jiffies_64() - s1Time) < msecs_to_jiffies(200)) {
        state = 2;
        s1Time = get_jiffies_64();
      }else{
        state = 0;
      }
      break;

    case 3:
      input_report_key(priv->input, BTN_LEFT, 0);
      input_sync(priv->input);
      state = 0;
      break;
    }
  }

  if ((state == 2) && ((get_jiffies_64() - s1Time) > msecs_to_jiffies(200))) {
    state = 0;
    input_report_key(priv->input, BTN_LEFT, 1);
    input_sync(priv->input);
    input_report_key(priv->input, BTN_LEFT, 0);
    input_sync(priv->input);
  }

  // if((xpos != 0) && (ypos != 0)){
  //     //input_report_key(priv->input, BTN_TOUCH, 1);
  //     //input_report_abs(priv->input, ABS_X, xpos);
  //     //input_report_abs(priv->input, ABS_Y, ypos);
  //     dx = xpos - lastx;
  //     dy = ypos - lasty;

  //     lastx = xpos;
  //     lasty = ypos;

  //     if(state == 0)
  //     {
  //         state = state + 1;
  //         s1Time = get_jiffies_64();
  //         goto out;
  //     }

  //     //input_report_key(priv->input, BTN_TOUCH, 0);
  //     input_report_rel(priv->input, REL_X, dx);
  //     input_report_rel(priv->input, REL_Y, dy);
  //     input_sync(priv->input);

  // }else{
  //     state = 0;
  //     if((get_jiffies_64() - s1Time) < msecs_to_jiffies(200))
  //     {
  //         //printk("click\r\n");
  //         if((dx == 0) && (dy == 0)){
  //             input_report_key(priv->input, BTN_LEFT, 1);
  //             input_sync(priv->input);
  //             input_report_key(priv->input, BTN_LEFT, 0);
  //             input_sync(priv->input);
  //             s1Time = 0;
  //         }
  //     }else{

  //     }
  //     //lastx = 0;
  //     //lasty = 0;
  //     //input_report_key(priv->input, BTN_TOUCH, 0);
  //     //input_sync(priv->input);
  // }

out:

  schedule_delayed_work(&priv->work, msecs_to_jiffies(2));
  return;
}

static int i2c_tp_probe(struct i2c_client *client,
                        const struct i2c_device_id *idp) {
  struct i2c_ts_priv *priv;
  struct input_dev *input;
  int error;
  // char buf[2];

  priv = kzalloc(sizeof(*priv), GFP_KERNEL);
  if (!priv) {
    dev_err(&client->dev, "failed to allocate driver data\n");
    error = -ENOMEM;
    goto err0;
  }

  dev_set_drvdata(&client->dev, priv);

  input = input_allocate_device();
  if (!input) {
    dev_err(&client->dev, "Failed to allocate input device.\n");
    error = -ENOMEM;
    goto err1;
  }

  input->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REL);
  input->keybit[BIT_WORD(BTN_LEFT)] = BIT_MASK(BTN_LEFT) | BIT_MASK(BTN_RIGHT);
  input->relbit[0] = BIT_MASK(REL_X) | BIT_MASK(REL_Y);

  input->name = client->name;
  input->id.bustype = BUS_I2C;
  input->dev.parent = &client->dev;

  input->open = i2c_tp_open;
  input->close = i2c_tp_close;

  input_set_drvdata(input, priv);

  priv->client = client;
  priv->input = input;
  INIT_DELAYED_WORK(&priv->work, i2c_tp_poscheck);
  priv->irq = client->irq;

  error = input_register_device(input);
  if (error)
    goto err1;

  device_init_wakeup(&client->dev, 1);

  schedule_delayed_work(&priv->work, msecs_to_jiffies(2));

  return 0;

err1:
  input_free_device(input);
  kfree(priv);
err0:
  dev_set_drvdata(&client->dev, NULL);
  return error;
}

static void i2c_tp_remove(struct i2c_client *client) {
  struct i2c_ts_priv *priv = dev_get_drvdata(&client->dev);

  input_unregister_device(priv->input);
  kfree(priv);

  dev_set_drvdata(&client->dev, NULL);
  return;
}

static const struct i2c_device_id i2c_ts_id[] = {{"ubi-tp-i2c", 0}, {}};

MODULE_DEVICE_TABLE(i2c, i2c_ts_id);

static struct i2c_driver ubi_touchpad_i2c_driver = {
    .driver =
        {
            .name = "ubi-tp-i2c",
        },

    .probe = i2c_tp_probe,
    .remove = i2c_tp_remove,

    .id_table = i2c_ts_id,
};

module_i2c_driver(ubi_touchpad_i2c_driver);

MODULE_DESCRIPTION("UBISURFER I2C touchpad driver");
MODULE_LICENSE("GPL");
