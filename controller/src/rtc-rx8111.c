#include <linux/bcd.h>
#include <linux/bitops.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rtc.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
/* Bug fixed in 2021/APR/6 */
/* Insert and Defined ALARM_AE *
/* Basic time and calendar register */
#define RX8111_SEC			0x10
#define RX8111_MIN     			0x11
#define RX8111_HOUR    			0x12
#define RX8111_WEEK			0x13
#define RX8111_DAY			0x14
#define RX8111_MONTH   			0x15
#define RX8111_YEAR    			0x16
#define RX8111_MIN_ALARM		0x17
#define RX8111_HOUR_ALARM  		0x18
#define RX8111_WEEK_DAY_ALARM  		0x19
#define RX8111_TIMER_COUNTER0 		0x1A
#define RX8111_TIMER_COUNTER1 		0x1B
#define RX8111_TIMER_COUNTER2 		0x1C
#define RX8111_EXTENREG			0x1D
#define RX8111_FLAGREG    		0x1E
#define RX8111_CTRLREG			0x1F

#define RX8111_TIMESTAMP_1_1000S	0x20
#define RX8111_TIMESTAMP_1_100S		0x21
#define RX8111_TIMESTAMP_SEC		0x22
#define RX8111_TIMESTAMP_MIN		0x23
#define RX8111_TIMESTAMP_HOUR		0x24
#define RX8111_TIMESTAMP_WEEK		0x25
#define RX8111_TIMESTAMP_DAY		0x26
#define RX8111_TIMESTAMP_MONTHS		0x27
#define RX8111_TIMESTAMP_YEAR		0x28
#define RX8111_STATUS_STAMP		0x29

#define RX8111_EVIN_SETTING   		0x2B
#define RX8111_SEC_ALARM  		0x2C
#define RX8111_TIMER_CONTROL  		0x2D
#define RX8111_CMD_TRIG_CTRL  		0x2E
#define RX8111_COMMAND_TRIGGER 		0x2F

#define RX8111_PWR_SWITCH_CTRL 		0x32
#define RX8111_STATUS_MONITOR 		0x33
#define RX8111_TIME_STAMP_BUF_CTRL	0x34
#define RX8111_TIME_STAMP_TRIG_CTRL	0x35
#define RX8111_TIME_STAMP_DATA_STATUS	0x36

#define RX8111_EXT_TSEL0	BIT(0)
#define RX8111_EXT_TSEL1	BIT(1)
#define RX8111_EXT_ECP		BIT(2)
#define RX8111_EXT_WADA		BIT(3)
#define RX8111_EXT_TE 	 	BIT(4)
#define RX8111_EXT_USEL  	BIT(5)
#define RX8111_EXT_FSEL0	BIT(6)
#define RX8111_EXT_FSEL1  	BIT(7)

#define RX8111_FLAG_FSTOPF	BIT(0)
#define RX8111_FLAG_VLF		BIT(1)
#define RX8111_FLAG_EVF		BIT(2)
#define RX8111_FLAG_AF		BIT(3)
#define RX8111_FLAG_TF		BIT(4)
#define RX8111_FLAG_UF		BIT(5)
#define RX8111_FLAG_PORF	BIT(7)

#define RX8111_CTRL_STOP  	BIT(0)
#define RX8111_CTRL_EIE  	BIT(2)
#define RX8111_CTRL_AIE  	BIT(3)
#define RX8111_CTRL_TIE  	BIT(4)
#define RX8111_CTRL_UIE  	BIT(5)

#define RX8111_EVIN_EOVEN  	BIT(1)
#define RX8111_EVIN_EPRUP_SEL0 	BIT(2)
#define RX8111_EVIN_EPRUP_SEL1 	BIT(3)
#define RX8111_EVIN_EPRDW_SEL 	BIT(4)
#define RX8111_EVIN_ET0 	BIT(5)
#define RX8111_EVIN_ET1  	BIT(6)
#define RX8111_EVIN_EHL  	BIT(7)

#define RX8111_TIMER_CTRL_TSTP	BIT(0)
#define RX8111_TIMER_CTRL_TMPIN	BIT(1)
#define RX8111_TIMER_CTRL_TBKE	BIT(2)
#define RX8111_TIMER_CTRL_TBKON	BIT(3)

#define RX8111_CMD_TRIG_DUMMY0	BIT(0)
#define RX8111_CMD_TRIG_DUMMY1	BIT(1)
#define RX8111_CMD_TRIG_DUMMY2	BIT(2)
#define RX8111_CMD_TRIG_DUMMY3	BIT(3)
#define RX8111_CMD_TRIG_DUMMY4	BIT(4)
#define RX8111_CMD_TRIG_DUMMY5	BIT(5)
#define RX8111_CMD_TRIG_DUMMY6	BIT(6)
#define RX8111_CMD_TRIG_DUMMY7	BIT(7)

#define RX8111_PSC_SMP_TSEL0	BIT(0)
#define RX8111_PSC_SMP_TSEL1	BIT(1)
#define RX8111_PSC_SMP_SWSEL0	BIT(2)
#define RX8111_PSC_SMP_SWSEL1	BIT(3)
#define RX8111_PSC_SMP_INIEN	BIT(6)
#define RX8111_PSC_SMP_CHGEN	BIT(7)

#define RX8111_PSC_SMP_CHGEN	BIT(7)

#define RX8111_STAT_M_FVLOW	BIT(1)
#define RX8111_STAT_M_FVCMP	BIT(3)
#define RX8111_STAT_M_EVINMON	BIT(6) 
		/* Insert and Defined ALARM_AE */
#define RX8111_ALARM_AE         BIT(7) 

static const struct i2c_device_id rx8111_id[] = {
	{ "rx8111", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, rx8111_id);

static const struct of_device_id rx8111_of_match[] = {
	{ .compatible = "epson,rx8111" },
	{ }
};
MODULE_DEVICE_TABLE(of, rx8111_of_match);

struct rx8111_data {
	struct i2c_client *client;
	struct rtc_device *rtc;
	u8 ctrlreg;
};

static irqreturn_t rx8111_irq_1_handler(int irq, void *dev_id)
{
	struct i2c_client *client = dev_id;
	struct rx8111_data *rx8111 = i2c_get_clientdata(client);
	int flagreg;

	mutex_lock(&rx8111->rtc->ops_lock);

	flagreg = i2c_smbus_read_byte_data(client, RX8111_FLAGREG);

	if (flagreg <= 0) {
		mutex_unlock(&rx8111->rtc->ops_lock);
		return IRQ_NONE;
	}

	if (flagreg & RX8111_FLAG_VLF)
		dev_warn(&client->dev, "IRQ Frequency stop detected\n");

	if (flagreg & RX8111_FLAG_TF) {
		flagreg &= ~RX8111_FLAG_TF;
		rtc_update_irq(rx8111->rtc, 1, RTC_PF | RTC_IRQF);
	}

	if (flagreg & RX8111_FLAG_AF) {
		flagreg &= ~RX8111_FLAG_AF;
		rtc_update_irq(rx8111->rtc, 1, RTC_AF | RTC_IRQF);
	}

	if (flagreg & RX8111_FLAG_UF) {
		flagreg &= ~RX8111_FLAG_UF;
		rtc_update_irq(rx8111->rtc, 1, RTC_UF | RTC_IRQF);
	}

	i2c_smbus_write_byte_data(client, RX8111_FLAGREG, flagreg);

	mutex_unlock(&rx8111->rtc->ops_lock);
	return IRQ_HANDLED;
}

static int rx8111_get_time(struct device *dev, struct rtc_time *dt)
{
	struct rx8111_data *rx8111 = dev_get_drvdata(dev);
	u8 date[7];
	int err;

	err = i2c_smbus_read_i2c_block_data(rx8111->client, RX8111_SEC,7, date);
	if (err != 7)
		return err < 0 ? err : -EIO;

	dt->tm_sec = bcd2bin(date[RX8111_SEC - RX8111_SEC] & 0x7f);
	dt->tm_min = bcd2bin(date[RX8111_MIN - RX8111_SEC] & 0x7f);
	dt->tm_hour = bcd2bin(date[RX8111_HOUR - RX8111_SEC] & 0x3f);
	dt->tm_mday = bcd2bin(date[RX8111_DAY - RX8111_SEC] & 0x3f);
	dt->tm_mon = bcd2bin(date[RX8111_MONTH - RX8111_SEC] & 0x1f) - 1;
	dt->tm_year = bcd2bin(date[RX8111_YEAR - RX8111_SEC]) + 100;
	dt->tm_wday = ffs(date[RX8111_WEEK - RX8111_SEC] & 0x7f);

	return rtc_valid_tm(dt);
}

static int rx8111_set_time(struct device *dev, struct rtc_time *dt)
{
	struct rx8111_data *rx8111 = dev_get_drvdata(dev);
	u8 date[7];
	int ctrl;
	int ret;

	if ((dt->tm_year < 100) || (dt->tm_year > 199))
		return -EINVAL;

	/* set STOP bit to "1" to prevent imter update in time setting. */
	ctrl = i2c_smbus_read_byte_data(rx8111->client, RX8111_CTRLREG);		
	if (ctrl < 0)
		return ctrl;

	rx8111->ctrlreg = ctrl | RX8111_CTRL_STOP;	
	ret = i2c_smbus_write_byte_data(rx8111->client, RX8111_CTRLREG,rx8111->ctrlreg);
	if (ret < 0)
		return ret;

	date[RX8111_SEC - RX8111_SEC] = bin2bcd(dt->tm_sec);
	date[RX8111_MIN - RX8111_SEC] = bin2bcd(dt->tm_min);
	date[RX8111_HOUR - RX8111_SEC] = bin2bcd(dt->tm_hour);
	date[RX8111_DAY - RX8111_SEC] = bin2bcd(dt->tm_mday);
	date[RX8111_MONTH - RX8111_SEC] = bin2bcd(dt->tm_mon + 1);
	date[RX8111_YEAR - RX8111_SEC] = bin2bcd(dt->tm_year - 100);
	date[RX8111_WEEK - RX8111_SEC] = bin2bcd(1 << dt->tm_wday);

	ret = i2c_smbus_write_i2c_block_data(rx8111->client,
					     RX8111_SEC, 7, date);
	if (ret < 0)
		return ret;

	/* set STOP bit to "0" to prevent imter update in time setting. */
	ctrl = i2c_smbus_read_byte_data(rx8111->client, RX8111_CTRLREG);		
	if (ctrl < 0)
		return ctrl;
	rx8111->ctrlreg = 0;		
	ret = i2c_smbus_write_byte_data(rx8111->client, RX8111_CTRLREG,rx8111->ctrlreg);
	if (ret < 0)
		return ret;

	return 0;
}
static int rx8111_init_client(struct i2c_client *client)
{
	int err;
	int ctrl,ret;
	int extreg,ctrlreg;
	struct rtc_time dt;

	/* Initialize reserved registers as specified in Inspection sheet */
		
	err = i2c_smbus_write_byte_data(client, RX8111_PWR_SWITCH_CTRL, 0x02);	//0x32, bit2 = 1
	if (err < 0)
		return err;

	err = i2c_smbus_write_byte_data(client, RX8111_EXTENREG, 0x00);		//0x1D
	if (err < 0)
		return err;
	err = i2c_smbus_write_byte_data(client, RX8111_FLAGREG, 0x00);	//0x1E
	if (err < 0)
		return err;
	
	err = i2c_smbus_write_byte_data(client, RX8111_CTRLREG, 0x00);	//0x1F
	if (err < 0)
		return err;
	
	//Set the present time
	err = rx8111_set_time(&client->dev,&dt);	
	if (err <0)
		return err;

	//Setting the Alarm fucntion	AIE = 0
	ctrl = i2c_smbus_read_byte_data(client, RX8111_CTRLREG);
	if (ctrl < 0)
		return ctrl;

	ctrlreg = ctrl | RX8111_CTRL_AIE;
	ret = i2c_smbus_write_byte_data(client, RX8111_CTRLREG,ctrlreg);
	if (ret < 0)
		return ret;

	//Setting the Timer fucntion EXTEN REG TE = 0
	extreg = i2c_smbus_read_byte_data(client, RX8111_EXTENREG);
	if (extreg < 0)
		return extreg;

	extreg |= RX8111_EXT_TE;
	err = i2c_smbus_write_byte_data(client, RX8111_EXTENREG, extreg);
	if (err < 0)
		return err;

	//Setting the Timer fucntion TIE = 0
	ctrl = i2c_smbus_read_byte_data(client, RX8111_CTRLREG);
	if (ctrl < 0)
		return ctrl;

	ctrlreg = ctrl | RX8111_CTRL_TIE;
	ret = i2c_smbus_write_byte_data(client, RX8111_CTRLREG,ctrlreg);
	if (ret < 0)
		return ret;

	return err;
}

static int rx8111_read_alarm(struct device *dev, struct rtc_wkalrm *t)
{
	struct rx8111_data *rx8111 = dev_get_drvdata(dev);
	struct i2c_client *client = rx8111->client;
	u8 alarmvals[3];
	int flagreg;
	int err;

	err = i2c_smbus_read_i2c_block_data(client, RX8111_MIN_ALARM, 3, alarmvals);
	if (err != 3)
		return err < 0 ? err : -EIO;

	flagreg = i2c_smbus_read_byte_data(client, RX8111_FLAGREG);
	if (flagreg < 0)
		return flagreg;

	t->time.tm_sec = 0;
	t->time.tm_min = bcd2bin(alarmvals[0] & 0x7f);
	t->time.tm_hour = bcd2bin(alarmvals[1] & 0x3f);

	if (!(alarmvals[2] & RX8111_ALARM_AE))
		t->time.tm_mday = bcd2bin(alarmvals[2] & 0x7f);

	t->enabled = !!(rx8111->ctrlreg & RX8111_CTRL_AIE);
	t->pending = (flagreg & RX8111_FLAG_AF) && t->enabled;

	return err;
}

static int rx8111_set_alarm(struct device *dev, struct rtc_wkalrm *t)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rx8111_data *rx8111 = dev_get_drvdata(dev);
	u8 alarmvals[3];
	int extreg, flagreg;
	int err;

	flagreg = i2c_smbus_read_byte_data(client, RX8111_FLAGREG);
	if (flagreg < 0) {
		return flagreg;
	}

	if (rx8111->ctrlreg & (RX8111_CTRL_AIE | RX8111_CTRL_UIE)) {
		rx8111->ctrlreg &= ~(RX8111_CTRL_AIE | RX8111_CTRL_UIE);
		err = i2c_smbus_write_byte_data(rx8111->client, RX8111_CTRLREG,
						rx8111->ctrlreg);
		if (err < 0) {
			return err;
		}
	}

	flagreg &= ~RX8111_FLAG_AF;
	err = i2c_smbus_write_byte_data(rx8111->client, RX8111_FLAGREG, flagreg);
	if (err < 0)
		return err;

	alarmvals[0] = bin2bcd(t->time.tm_min);
	alarmvals[1] = bin2bcd(t->time.tm_hour);
	alarmvals[2] = bin2bcd(t->time.tm_mday);

	err = i2c_smbus_write_i2c_block_data(rx8111->client, RX8111_MIN_ALARM,
					     2, alarmvals);
	if (err < 0)
		return err;

	extreg = i2c_smbus_read_byte_data(client, RX8111_EXTENREG);
	if (extreg < 0)
		return extreg;

	extreg |= RX8111_EXT_WADA;
	err = i2c_smbus_write_byte_data(rx8111->client, RX8111_EXTENREG, extreg);
	if (err < 0)
		return err;

	if (alarmvals[2] == 0)
		alarmvals[2] |= RX8111_ALARM_AE;

	err = i2c_smbus_write_byte_data(rx8111->client, RX8111_WEEK_DAY_ALARM,
					alarmvals[2]);
	if (err < 0)
		return err;

	if (t->enabled) {
		if (rx8111->rtc->uie_rtctimer.enabled)
			rx8111->ctrlreg |= RX8111_CTRL_UIE;
		if (rx8111->rtc->aie_timer.enabled)
			rx8111->ctrlreg |=
				(RX8111_CTRL_AIE | RX8111_CTRL_UIE);

		err = i2c_smbus_write_byte_data(rx8111->client, RX8111_CTRLREG,
						rx8111->ctrlreg);
		if (err < 0)
			return err;
	}

	return 0;
}

static int rx8111_alarm_irq_enable(struct device *dev,unsigned int enabled)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rx8111_data *rx8111 = dev_get_drvdata(dev);
	int flagreg;
	u8 ctrl;
	int err;

	ctrl = rx8111->ctrlreg;

	if (enabled) 
	{
		if (rx8111->rtc->uie_rtctimer.enabled)
			ctrl |= RX8111_CTRL_UIE;
		if (rx8111->rtc->aie_timer.enabled)
			ctrl |= (RX8111_CTRL_AIE | RX8111_CTRL_UIE);
	} 
	else 
	{
		if (!rx8111->rtc->uie_rtctimer.enabled)
			ctrl &= ~RX8111_CTRL_UIE;
		if (!rx8111->rtc->aie_timer.enabled)
			ctrl &= ~RX8111_CTRL_AIE;
	}

	flagreg = i2c_smbus_read_byte_data(client, RX8111_FLAGREG);
	if (flagreg < 0)
		return flagreg;

	flagreg &= ~RX8111_FLAG_AF;
	err = i2c_smbus_write_byte_data(rx8111->client, RX8111_FLAGREG, flagreg);
	if (err < 0)
		return err;

	if (ctrl != rx8111->ctrlreg) {
		rx8111->ctrlreg = ctrl;
		err = i2c_smbus_write_byte_data(rx8111->client, RX8111_CTRLREG,rx8111->ctrlreg);
		if (err < 0)
			return err;
	}

	return 0;
}

static int rx8111_ioctl(struct device *dev, unsigned int cmd, unsigned long arg)
{
	struct rx8111_data *rx8111 = dev_get_drvdata(dev);
	struct rtc_wkalrm alarm;
	struct rtc_time *dt;
	int err = 0;

        err = mutex_lock_interruptible(&rx8111->rtc->ops_lock);
        if (err)
                return err;

	switch (cmd) {

        case RTC_AIE_ON:	
                mutex_unlock(&rx8111->rtc->ops_lock);
                return rx8111_alarm_irq_enable(dev, 1);

        case RTC_AIE_OFF:	
                mutex_unlock(&rx8111->rtc->ops_lock);
                return rx8111_alarm_irq_enable(dev, 0);

        case RTC_UIE_ON:	
                mutex_unlock(&rx8111->rtc->ops_lock);
                return rtc_update_irq_enable(rx8111->rtc, 1);

        case RTC_UIE_OFF:	
                mutex_unlock(&rx8111->rtc->ops_lock);
                return rtc_update_irq_enable(rx8111->rtc, 0);

        case RTC_PIE_ON:
                err = rtc_irq_set_state(rx8111->rtc, NULL, 1);
		return 0;

        case RTC_PIE_OFF:
                err = rtc_irq_set_state(rx8111->rtc, NULL, 0);
		return 0;

	case RTC_ALM_READ:
                mutex_unlock(&rx8111->rtc->ops_lock);

                err = rtc_read_alarm(rx8111->rtc, &alarm);
		if (err < 0)
			return err;
		if (copy_to_user((void __user *)arg, &alarm.time, sizeof(dt)))
			err = -EFAULT;
		return err;

	case RTC_ALM_SET:
		mutex_unlock(&rx8111->rtc->ops_lock);

		if (copy_from_user(&alarm.time, (void __user *)arg, sizeof(dt)))
			return -EFAULT;

		alarm.enabled = 0;
		alarm.pending = 0;
		alarm.time.tm_wday = -1;
		alarm.time.tm_yday = -1;
		alarm.time.tm_isdst = -1;

		{
			time64_t now, then;

			err = rtc_read_time(rx8111->rtc, dt);
			if (err < 0)
				return err;
			now = rtc_tm_to_time64(dt);

			alarm.time.tm_mday = dt->tm_mday;
			alarm.time.tm_mon = dt->tm_mon;
			alarm.time.tm_year = dt->tm_year;
			err  = rtc_valid_tm(&alarm.time);
			if (err < 0)
				return err;
			then = rtc_tm_to_time64(&alarm.time);

			/* alarm may need to wrap into tomorrow */
			if (then < now) {
				rtc_time64_to_tm(now + 24 * 60 * 60, dt);
				alarm.time.tm_mday = dt->tm_mday;
				alarm.time.tm_mon = dt->tm_mon;
				alarm.time.tm_year = dt->tm_year;
			}
		}

		return rtc_set_alarm(rx8111->rtc, &alarm);

	default:
		return -ENOIOCTLCMD;
	}
}

static struct rtc_class_ops rx8111_rtc_ops = {
	.read_time = rx8111_get_time,
	.set_time = rx8111_set_time,
	.ioctl = rx8111_ioctl,
	.alarm_irq_enable = rx8111_alarm_irq_enable,
};

static int rx8111_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct rx8111_data *rx8111;
	
	int err = 0;
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA
		| I2C_FUNC_SMBUS_I2C_BLOCK)) {
		dev_err(&adapter->dev, "doesn't support required functionality\n");
		return -EIO;
	}

	rx8111 = devm_kzalloc(&client->dev, sizeof(struct rx8111_data),
			      GFP_KERNEL);
	if (!rx8111)
		return -ENOMEM;

	rx8111->client = client;
	i2c_set_clientdata(client, rx8111);

	err = rx8111_init_client(client);
	if (err)
		return err;
	else
		dev_info(&client->dev, "Init Success\n");


	if (client->irq > 0) {
		dev_info(&client->dev, "IRQ %d supplied\n", client->irq);
		err = devm_request_threaded_irq(&client->dev, client->irq, NULL,
						rx8111_irq_1_handler,
						IRQF_TRIGGER_LOW | IRQF_ONESHOT,
						"rx8111", client);

		if (err) {
			dev_err(&client->dev, "unable to request IRQ\n");
			client->irq = 0;
		} else {
			rx8111_rtc_ops.read_alarm = rx8111_read_alarm;
			rx8111_rtc_ops.set_alarm = rx8111_set_alarm;
			rx8111_rtc_ops.alarm_irq_enable = rx8111_alarm_irq_enable;
		}
	}
	else
	dev_info(&client->dev, "IRQ supplied fail\n");
	rx8111->rtc = devm_rtc_device_register(&client->dev, client->name,
		&rx8111_rtc_ops, THIS_MODULE);

	if (IS_ERR(rx8111->rtc)) {
		dev_err(&client->dev, "unable to register the class device\n");
		return PTR_ERR(rx8111->rtc);
	}

	rx8111->rtc->max_user_freq = 1;

	return err;
}
static int rx8111_remove(struct i2c_client *client)
{
	dev_info(&client->dev, "rx8111 remove\n");
        return 0;
}

static struct i2c_driver rx8111_driver = {
	.driver = {
		.name = "rtc-rx8111",
		.of_match_table = of_match_ptr(rx8111_of_match),
	},
	.probe		= rx8111_probe,
	.id_table	= rx8111_id,
	.remove		= rx8111_remove,
};
module_i2c_driver(rx8111_driver);

MODULE_AUTHOR("Sam Kim <sam@varikorea.co.kr>");
MODULE_DESCRIPTION("Epson RX8111 RTC driver");
MODULE_LICENSE("GPL v2");

