/* LGE_CHANGES LGE_RAPI_COMMANDS  */
/* Created by khlee@lge.com  
 * arch/arm/mach-msm/lge/LG_rapi_client.c
 *
 * Copyright (C) 2009 LGE, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/slab.h> 
#include <linux/kernel.h>
#include <linux/err.h>
#include <mach/oem_rapi_client.h>
#include <mach/lge_diag_testmode.h>
#ifdef CONFIG_LGE_DIAG_ICD
#include <mach/lge_diag_icd.h>
#endif

#ifdef CONFIG_MACH_LGE_M3S
#include <mach/msm_battery_m3s.h>
#else /*CONFIG_MACH_LGE_M3S*/
#include <mach/msm_battery.h>
#endif /*CONFIG_MACH_LGE_M3S*/

#ifdef CONFIG_LGE_DIAG_SRD
#include <mach/lge_diag_srd.h>
#include <mach/userDataBackUpDiag.h>
#endif

/* BEGIN: 0014166 jihoon.lee@lge.com 20110116 */
/* MOD 0014166: [KERNEL] send_to_arm9 work queue */
#include <linux/kmod.h>
#include <linux/workqueue.h>
/* END: 0014166 jihoon.lee@lge.com 20110116 */

/* BEGIN: 0015327 jihoon.lee@lge.com 20110204 */
/* MOD 0015327: [KERNEL] LG RAPI validity check */
// remove unnecessary kernel logs
#define LOCAL_RAPI_DBG
/* END: 0015327 jihoon.lee@lge.com 20110204 */

#define GET_INT32(buf)       (int32_t)be32_to_cpu(*((uint32_t*)(buf)))
#define PUT_INT32(buf, v)        (*((uint32_t*)buf) = (int32_t)be32_to_cpu((uint32_t)(v)))
#define GET_U_INT32(buf)         ((uint32_t)GET_INT32(buf))
#define PUT_U_INT32(buf, v)      PUT_INT32(buf, (int32_t)(v))

#define GET_LONG(buf)            ((long)GET_INT32(buf))
#define PUT_LONG(buf, v) \
	(*((u_long*)buf) = (long)be32_to_cpu((u_long)(v)))

#define GET_U_LONG(buf)	      ((u_long)GET_LONG(buf))
#define PUT_U_LONG(buf, v)	      PUT_LONG(buf, (long)(v))


#define GET_BOOL(buf)            ((bool_t)GET_LONG(buf))
#define GET_ENUM(buf, t)         ((t)GET_LONG(buf))
#define GET_SHORT(buf)           ((short)GET_LONG(buf))
#define GET_U_SHORT(buf)         ((u_short)GET_LONG(buf))

#define PUT_ENUM(buf, v)         PUT_LONG(buf, (long)(v))
#define PUT_SHORT(buf, v)        PUT_LONG(buf, (long)(v))
#define PUT_U_SHORT(buf, v)      PUT_LONG(buf, (long)(v))

#define LG_RAPI_CLIENT_MAX_OUT_BUFF_SIZE 128
#define LG_RAPI_CLIENT_MAX_IN_BUFF_SIZE 128

/* BEGIN: 0014110 jihoon.lee@lge.com 20110115 */
/* MOD 0014110: [FACTORY RESET] stability */
/* sync up with oem_rapi */
extern uint32_t get_oem_rapi_open_cnt(void);

//static uint32_t open_count = 0; // confirm
/* END: 0014110 jihoon.lee@lge.com 20110115 */

static struct msm_rpc_client *client;

/* BEGIN: 0014166 jihoon.lee@lge.com 20110116 */
/* MOD 0014166: [KERNEL] send_to_arm9 work queue */
extern void msleep(unsigned int msecs);

static struct workqueue_struct *send_to_arm9_wq = NULL;
struct __send_to_arm9 {
    int mutex_flag;
    boolean complete_main;
    boolean complete_sub;
    void*	pReq;
    void* pRsp;
	unsigned int rsp_len;
    struct work_struct work;
};
static struct __send_to_arm9 send_to_arm9_data;

static void send_to_arm9_wq_func(struct work_struct *work);

typedef enum {
    NORMAL_WORK_FLAG = 0,
    WORK_QUEUE_FLAG = 1,
}send_to_arm9_wq_type;

#define INCREASE_MUTEX_FLAG()         (++send_to_arm9_data.mutex_flag)
#define DECREASE_MUTEX_FLAG()         ((send_to_arm9_data.mutex_flag==0)?(send_to_arm9_data.mutex_flag=0):(send_to_arm9_data.mutex_flag--))
#define GET_MUTEX_FLAG()	(send_to_arm9_data.mutex_flag)
#define SET_MUTEX_FLAG(v)	(send_to_arm9_data.mutex_flag = v)

#define GET_COMPLETE_MAIN()	(send_to_arm9_data.complete_main==0)?0:1
#define SET_COMPLETE_MAIN(v)	(send_to_arm9_data.complete_main = v)
#define GET_COMPLETE_SUB()		(send_to_arm9_data.complete_sub==0)?0:1
#define SET_COMPLETE_SUB(v)	(send_to_arm9_data.complete_sub = v)
/* END: 0014166 jihoon.lee@lge.com 20110116 */

/* BEGIN: 0015327 jihoon.lee@lge.com 20110204 */
/* MOD 0015327: [KERNEL] LG RAPI validity check */
typedef enum{
	LG_RAPI_SUCCESS = 0,
	LG_RAPI_OVER_UNDER_FLOW = 1,
	LG_RAPI_INVALID_RESPONSE = 2,
	
}lg_rapi_status_type;
/* END: 0015327 jihoon.lee@lge.com 20110204 */

int LG_rapi_init(void)
{
	client = oem_rapi_client_init();
	if (IS_ERR(client)) {
		pr_err("%s: couldn't open oem rapi client\n", __func__);
		return PTR_ERR(client);
	}
/* BEGIN: 0014110 jihoon.lee@lge.com 20110115 */
/* MOD 0014110: [FACTORY RESET] stability */
/* sync up with oem_rapi */
	//open_count++;
/* END: 0014110 jihoon.lee@lge.com 20110115 */

	return 0;
}

void Open_check(void)
{
/* BEGIN: 0014110 jihoon.lee@lge.com 20110115 */
/* MOD 0014110: [FACTORY RESET] stability */
/* sync up with oem_rapi */
	uint32_t open_count;
	
	/* to double check re-open; */
	open_count = get_oem_rapi_open_cnt();
	
	if(open_count > 0)
	{
#ifdef LOCAL_RAPI_DBG
		pr_info("%s,  open_count : %d \r\n", __func__, open_count);
#endif
		return;
	}

/* END: 0014110 jihoon.lee@lge.com 20110115 */
	LG_rapi_init();
}

/* BEGIN: 0015327 jihoon.lee@lge.com 20110204 */
/* MOD 0015327: [KERNEL] LG RAPI validity check */
int lg_rapi_check_validity_and_copy_result(void* src, char* dest, uint32 size_expected)
{
	struct oem_rapi_client_streaming_func_ret* psrc = (struct oem_rapi_client_streaming_func_ret*)src;
	int result = -1;

	// error handling - if rpc timeout occurs, page fault will be invoked
	if((psrc->output != NULL) && (psrc->out_len != NULL) && (*(psrc->out_len) > 0))
	{
		// check size overflow or underflow
		if(*(psrc->out_len) == size_expected)
		{
			memcpy((void *)dest, psrc->output, *(psrc->out_len));
			result = LG_RAPI_SUCCESS;
		}
		else
		{
			pr_err("%s, size overflow or underflow, expected : %u, returned : %d\r\n", __func__, size_expected, *(psrc->out_len));
			memcpy((void *)dest, psrc->output,size_expected >(*(psrc->out_len))?(*(psrc->out_len)):size_expected);
			result = LG_RAPI_OVER_UNDER_FLOW;
		}
	}
	else
	{
		pr_err("%s, invalid rpc results\n", __func__);
		result = LG_RAPI_INVALID_RESPONSE;
	}

	return result;
}
/* END: 0015327 jihoon.lee@lge.com 20110204 */

int msm_chg_LG_cable_type(void)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	char output[LG_RAPI_CLIENT_MAX_OUT_BUFF_SIZE];
	int rc= -1;
	int errCount= 0;

	Open_check();

/* LGE_CHANGES_S [younsuk.song@lge.com] 2010-09-06, Add error control code. Repeat 3 times if error occurs*/

	do 
	{
		arg.event = LG_FW_RAPI_CLIENT_EVENT_GET_LINE_TYPE;
		arg.cb_func = NULL;
		arg.handle = (void*) 0;
		arg.in_len = 0;
		arg.input = NULL;
		arg.out_len_valid = 1;
		arg.output_valid = 1;
		arg.output_size = 4;

		ret.output = NULL;
		ret.out_len = NULL;

		rc= oem_rapi_client_streaming_function(client, &arg, &ret);
	
		if (rc < 0)
			pr_err("get LG_cable_type error \r\n");
		else
			pr_info("msm_chg_LG_cable_type: %d \r\n", GET_INT32(ret.output));

	} while (rc < 0 && errCount++ < 3);

/* LGE_CHANGES_E [younsuk.song@lge.com] */

/* BEGIN: 0015327 jihoon.lee@lge.com 20110204 */
/* MOD 0015327: [KERNEL] LG RAPI validity check */
	memset(output, 0, LG_RAPI_CLIENT_MAX_OUT_BUFF_SIZE);

	rc = lg_rapi_check_validity_and_copy_result((void*)&ret, output, arg.output_size);
/* END: 0015327 jihoon.lee@lge.com 20110204 */

/* BEGIN: 0014591 jihoon.lee@lge.com 20110122 */
/* MOD 0014591: [LG_RAPI] rpc request heap leakage bug fix */
	// free received buffers if it is not empty
	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);
/* END: 0014591 jihoon.lee@lge.com 2011022 */

	return (GET_INT32(output));  
}

/* BEGIN: 0014166 jihoon.lee@lge.com 20110116 */
/* MOD 0014166: [KERNEL] send_to_arm9 work queue */
static void
do_send_to_arm9(void*	pReq, void* pRsp, int flag, unsigned int rsp_len)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int rc= -1;

	Open_check();
	printk(KERN_INFO "%s %s start\n", __func__, (flag==NORMAL_WORK_FLAG)?"[N]":"[WQ]");

	arg.event = LG_FW_TESTMODE_EVENT_FROM_ARM11;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = sizeof(DIAG_TEST_MODE_F_req_type);
	arg.input = (char*)pReq;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = rsp_len;

	ret.output = NULL;
	ret.out_len = NULL;

/* BEGIN: 0015327 jihoon.lee@lge.com 20110204 */
/* MOD 0015327: [KERNEL] LG RAPI validity check */
	rc= oem_rapi_client_streaming_function(client, &arg, &ret);
	if (rc < 0)
	{
		pr_err("%s, rapi reqeust failed\r\n", __func__);
		((DIAG_TEST_MODE_F_rsp_type*)pRsp)->ret_stat_code = TEST_FAIL_S;
		
	}
	else
	{
		rc = lg_rapi_check_validity_and_copy_result((void*)&ret, (char*)pRsp, arg.output_size);
		if(rc == LG_RAPI_INVALID_RESPONSE)
		{
			((DIAG_TEST_MODE_F_rsp_type*)pRsp)->ret_stat_code = TEST_FAIL_S;
		}
	}
/* END: 0015327 jihoon.lee@lge.com 20110204 */

/* BEGIN: 0014591 jihoon.lee@lge.com 20110122 */
/* MOD 0014591: [LG_RAPI] rpc request heap leakage bug fix */
	// free received buffers if it is not empty
	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);
/* END: 0014591 jihoon.lee@lge.com 2011022 */

	printk(KERN_INFO "%s %s end\n", __func__, (flag==NORMAL_WORK_FLAG)?"[N]":"[WQ]");
}

void wait_for_main_try_finished(void)
{
	int local_cnt = 0;
#ifdef LOCAL_RAPI_DBG
	printk(KERN_INFO "%s start\n", __func__);
#endif
	do
	{
		if(GET_COMPLETE_MAIN()==1)
			break;
		
		msleep(100);
		if(++local_cnt > 50)
		{
			printk(KERN_ERR "%s wait too long : %d sec\n", __func__, (local_cnt*100/1000));
			break;
		}
	}while(1);
#ifdef LOCAL_RAPI_DBG
	printk(KERN_INFO "%s end\n", __func__);
#endif
}

void wait_for_sub_try_finished(void)
{
	int local_cnt = 0;
#ifdef LOCAL_RAPI_DBG
	printk(KERN_INFO "%s start\n", __func__);
#endif
	do
	{
		if(GET_COMPLETE_SUB()==1)
			break;
		
		msleep(100);
		if(++local_cnt > 50)
		{
			printk(KERN_ERR "%s wait too long : %d sec\n", __func__, (local_cnt*100/1000));
			break;
		}
	}while(1);
#ifdef LOCAL_RAPI_DBG
	printk(KERN_INFO "%s end\n", __func__);
#endif
}

static void
send_to_arm9_wq_func(struct work_struct *work)
{
	printk(KERN_INFO "%s, flag : %d, handle work queue\n", __func__, GET_MUTEX_FLAG() );
	//INCREASE_MUTEX_FLAG(); //increase before getting into the work queue
	wait_for_main_try_finished();
	do_send_to_arm9(send_to_arm9_data.pReq, send_to_arm9_data.pRsp, WORK_QUEUE_FLAG, send_to_arm9_data.rsp_len);
	DECREASE_MUTEX_FLAG();
	SET_COMPLETE_SUB(1);
	return;
}

void send_to_arm9(void*	pReq, void* pRsp, unsigned int rsp_len)
{
	// sleep some time to avoid normal and work queue overlap
	msleep(100);
	
	// initialize this work queue only once
	if(send_to_arm9_wq == NULL)
	{
		printk(KERN_INFO "%s initialize work queue\n", __func__);
		send_to_arm9_wq = create_singlethread_workqueue("send_to_arm9_wq");
		INIT_WORK(&send_to_arm9_data.work, send_to_arm9_wq_func);
		SET_MUTEX_FLAG(0); // initial flag is 0
		SET_COMPLETE_MAIN(1); // initial complete flag is 1
		SET_COMPLETE_SUB(1); // initial complete flag is 1
		
	}

    if(GET_MUTEX_FLAG() == 0)
    {
#ifdef LOCAL_RAPI_DBG
       	printk(KERN_INFO "%s, flag : %d, do normal work\n", __func__, GET_MUTEX_FLAG());
#endif
       	INCREASE_MUTEX_FLAG();
		SET_COMPLETE_MAIN(0);
		wait_for_sub_try_finished();
       	do_send_to_arm9(pReq, pRsp, NORMAL_WORK_FLAG, rsp_len);
		SET_COMPLETE_MAIN(1);
		DECREASE_MUTEX_FLAG();
    }
	else
	{
		// hardly ever comes here in normal case.
		// Previously, it took more than 30 secs for modem factory reset, and this wq was needed at that time
		printk(KERN_INFO "%s, flag : %d, activate work queue\n", __func__, GET_MUTEX_FLAG());
		INCREASE_MUTEX_FLAG(); // increase before getting into the work queue
		SET_COMPLETE_SUB(0);
		send_to_arm9_data.pReq = pReq;
		send_to_arm9_data.pRsp = pRsp;
		send_to_arm9_data.rsp_len = rsp_len;
		queue_work(send_to_arm9_wq, &send_to_arm9_data.work);
		wait_for_sub_try_finished();
	}

	return;
}
/* END: 0014166 jihoon.lee@lge.com 20110116 */

// LGE_CHANGE_S [2011.02.08] [myeonggyu.son@lge.com] [gelato] add icd oem rapi function [START]
#ifdef CONFIG_LGE_DIAG_ICD
void icd_send_to_arm9(void*	pReq, void* pRsp, unsigned int output_length)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;

	Open_check();

	arg.event = LG_FW_RAPI_ICD_DIAG_EVENT;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = sizeof(DIAG_ICD_F_req_type);
	arg.input = (char*)pReq;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = output_length;

	ret.output = NULL;
	ret.out_len = NULL;
	
	oem_rapi_client_streaming_function(client, &arg, &ret);

	memcpy(pRsp,ret.output,*ret.out_len);

	return;
}
EXPORT_SYMBOL(icd_send_to_arm9);
#endif
// LGE_CHANGE_E [2011.02.08] [myeonggyu.son@lge.com] [gelato] add icd oem rapi function [END]

#ifdef CONFIG_LGE_DIAG_SRD
void remote_rpc_srd_cmmand(void*pReq, void* pRsp )  //kabjoo.choi
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	//char output[4];
	int rc= -1;
	//int request_cmd = command;

	Open_check();
	arg.event = LG_FW_REQUEST_SRD_RPC;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = sizeof(srd_req_type);
	arg.input = (char*)pReq;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = sizeof(srd_rsp_type);

	ret.output = (char*)NULL;
	ret.out_len = 0;

	rc = oem_rapi_client_streaming_function(client,&arg,&ret);
	if (rc < 0)
	{
		pr_err("%s, rapi reqeust failed\r\n", __func__);
		((srd_rsp_type*)pRsp)->header.err_code = UDBU_ERROR_CANNOT_COMPLETE;
		
	}
	else
	{
		rc = lg_rapi_check_validity_and_copy_result((void*)&ret, (char*)pRsp, arg.output_size);
		if(rc == LG_RAPI_INVALID_RESPONSE)
			((srd_rsp_type*)pRsp)->header.err_code = UDBU_ERROR_CANNOT_COMPLETE;
	}


	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);
	
	return ;
}
EXPORT_SYMBOL(remote_rpc_srd_cmmand);

//CSFB SRD
unsigned char pbuf_emmc[192]; // size of test_mode_emmc_direct_type

void remote_rpc_with_mdm(uint32 in_len, byte *input, uint32 *out_len, byte *output)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int rc= -1;

	printk(KERN_ERR "%s, start OEM_RAPI\n",__func__);

	Open_check();

	arg.event = LG_FW_OEM_RAPI_CLIENT_SRD_COMMAND;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len =  in_len;
	arg.input = input;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = sizeof(pbuf_emmc);

	ret.output = NULL;
	ret.out_len = NULL;

	rc = oem_rapi_client_streaming_function(client,&arg,&ret);
	
	if(ret.output == NULL || ret.out_len == NULL)
	{ 
		printk(KERN_ERR "%s, output is NULL\n",__func__);
		return;
	}
	
	printk(KERN_ERR "%s, output lenght =%dis\n",__func__,*ret.out_len);
	*out_len = *ret.out_len;
	memcpy(output, ret.output, *ret.out_len);

//=============================================================			

/* BEGIN: 0014591 jihoon.lee@lge.com 20110122 */
/* MOD 0014591: [LG_RAPI] rpc request heap leakage bug fix */
	// free received buffers if it is not empty
	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);
/* END: 0014591 jihoon.lee@lge.com 2011022 */
	
	return;
}
EXPORT_SYMBOL(remote_rpc_with_mdm);

#endif

void set_operation_mode(boolean info)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int rc= -1;

	Open_check();

	arg.event = LG_FW_SET_OPERATION_MODE;	
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = sizeof(boolean);
	arg.input = (char*) &info;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;

	ret.output = (char*) NULL;
	ret.out_len = 0;

	rc = oem_rapi_client_streaming_function(client,&arg, &ret);
	if (rc < 0)
	{
		pr_err("%s, rapi reqeust failed\r\n", __func__);
	}

/* BEGIN: 0014591 jihoon.lee@lge.com 20110122 */
/* MOD 0014591: [LG_RAPI] rpc request heap leakage bug fix */
	// free received buffers if it is not empty
	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);
/* END: 0014591 jihoon.lee@lge.com 2011022 */
}
EXPORT_SYMBOL(set_operation_mode);

void battery_info_get(struct batt_info* resp_buf)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	uint32_t out_len;
	int ret_val;
	struct batt_info rsp_buf;

	Open_check();

	arg.event = LG_FW_A2M_BATT_INFO_GET;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = 0;
	arg.input = NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = sizeof(rsp_buf);

	ret.output = (char*)&rsp_buf;
	ret.out_len = &out_len;

	ret_val = oem_rapi_client_streaming_function(client, &arg, &ret);
	if(ret_val == 0) {
		resp_buf->valid_batt_id = GET_U_INT32(&rsp_buf.valid_batt_id);
		resp_buf->batt_therm = GET_U_INT32(&rsp_buf.batt_therm);
		resp_buf->batt_temp = GET_INT32(&rsp_buf.batt_temp);
	} else { /* In case error */
		resp_buf->valid_batt_id = 1; /* authenticated battery id */
		resp_buf->batt_therm = 100;  /* 100 battery therm adc */
		resp_buf->batt_temp = 30;    /* 30 degree celcius */
	}
	return;
}
EXPORT_SYMBOL(battery_info_get);

void pseudo_batt_info_set(struct pseudo_batt_info_type* info)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int rc= -1;

	Open_check();

	arg.event = LG_FW_A2M_PSEUDO_BATT_INFO_SET;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = sizeof(struct pseudo_batt_info_type);
	arg.input = (char*)info;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;  /* alloc memory for response */

	ret.output = (char*)NULL;
	ret.out_len = 0;

	rc = oem_rapi_client_streaming_function(client, &arg, &ret);
	if (rc < 0)
	{
		pr_err("%s, rapi reqeust failed\r\n", __func__);
	}

/* BEGIN: 0014591 jihoon.lee@lge.com 20110122 */
/* MOD 0014591: [LG_RAPI] rpc request heap leakage bug fix */
	// free received buffers if it is not empty
	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);
/* END: 0014591 jihoon.lee@lge.com 2011022 */
	
	return;
}
EXPORT_SYMBOL(pseudo_batt_info_set);
void block_charging_set(int bypass)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int rc= -1;

	Open_check();
	arg.event = LG_FW_A2M_BLOCK_CHARGING_SET;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = sizeof(int);
	arg.input = (char*) &bypass;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;

	ret.output = (char*)NULL;
	ret.out_len = 0;

	rc = oem_rapi_client_streaming_function(client,&arg,&ret);
	if (rc < 0)
	{
		pr_err("%s, rapi reqeust failed\r\n", __func__);
	}

/* BEGIN: 0014591 jihoon.lee@lge.com 20110122 */
/* MOD 0014591: [LG_RAPI] rpc request heap leakage bug fix */
	// free received buffers if it is not empty
	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);
/* END: 0014591 jihoon.lee@lge.com 2011022 */
	
	return;
}
EXPORT_SYMBOL(block_charging_set);

#if 1	//def LG_FW_USB_ACCESS_LOCK
int lg_get_usb_lock_state(void)
{

	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	byte lock_state = 0xFF;
	unsigned int out_len = 0xFFFFFFFF;

	Open_check();

	arg.event = LG_FW_GET_USB_LOCK_STATE;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = 0;
	arg.input = NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = 1;

	ret.output = NULL;
	ret.out_len = NULL;

	oem_rapi_client_streaming_function(client, &arg, &ret);

	lock_state = *ret.output;
	out_len = *ret.out_len;

	return lock_state;
}
EXPORT_SYMBOL(lg_get_usb_lock_state);

void lg_set_usb_lock_state(int lock)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;

	Open_check();
	arg.event = LG_FW_SET_USB_LOCK_STATE;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = sizeof(int);
	arg.input = (char*) &lock;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;

	ret.output = (char*)NULL;
	ret.out_len = 0;

	oem_rapi_client_streaming_function(client,&arg,&ret);
	return;

}
EXPORT_SYMBOL(lg_set_usb_lock_state);

void lg_get_spc_code(char * spc_code)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	unsigned int out_len = 0xFFFFFFFF;

	Open_check();

	arg.event = LG_FW_GET_SPC_CODE;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = 0;
	arg.input = NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = 6;

	ret.output = NULL;
	ret.out_len = NULL;

	oem_rapi_client_streaming_function(client, &arg, &ret);


	if (out_len && ret.output)
	{
		out_len = *ret.out_len;
		memcpy(spc_code, ret.output, out_len);
		
		kfree(ret.output);
		kfree(ret.out_len);
	}
	else
		memset(spc_code, 0x0, 6);
	return;
}
EXPORT_SYMBOL(lg_get_spc_code);
#endif

void msm_get_MEID_type(char* sMeid)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
#if 1	/* LGE_CHANGES_S [moses.son@lge.com] 2011-02-09, to avoid compie error, need to check  */
	Open_check();

	arg.event = LG_FW_MEID_GET;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = 0;
	arg.input = NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = 15;

	ret.output = NULL;
	ret.out_len = NULL;

	oem_rapi_client_streaming_function(client, &arg, &ret);

	memcpy(sMeid,ret.output,14); 

	kfree(ret.output);
	kfree(ret.out_len);
#else
	memcpy(sMeid, "12345678912345", 14);
#endif
	return;  
}

#if 0
void msm_get_MEID_type(char* sMeid)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int rc= -1;
	
	char temp[16];
	memset(temp,0,16); // passing argument 2 of 'memset' makes integer from pointer without a cast, change NULL to 0
	
	Open_check();

	arg.event = LG_FW_MEID_GET;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	//FIX ME : RPC_ACCEPTSTAT_GARBAGE_ARGS rpc fail
	arg.in_len = sizeof(temp);
	arg.input = temp;
//	arg.in_len = 0;
//	arg.input = NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = 15;

	ret.output = NULL;
	ret.out_len = NULL;

/* BEGIN: 0015327 jihoon.lee@lge.com 20110204 */
/* MOD 0015327: [KERNEL] LG RAPI validity check */
	rc = oem_rapi_client_streaming_function(client, &arg, &ret);
	if (rc < 0)
	{
		pr_err("%s, rapi reqeust failed\r\n", __func__);
		memset(sMeid,0,14);
	}
	else
	{
		rc = lg_rapi_check_validity_and_copy_result((void*)&ret, (char*)sMeid, 14); // returned MEID size is 14
		if(rc == LG_RAPI_INVALID_RESPONSE)
			memset(sMeid,0,14);
		else
			printk(KERN_INFO "meid from modem nv : '%s'\n", sMeid);
	}
/* END: 0015327 jihoon.lee@lge.com 20110204 */

/* BEGIN: 0014591 jihoon.lee@lge.com 20110122 */
/* MOD 0014591: [LG_RAPI] rpc request heap leakage bug fix */
	// free received buffers if it is not empty
	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);
/* END: 0014591 jihoon.lee@lge.com 2011022 */

	return;  
}

EXPORT_SYMBOL(msm_get_MEID_type);
#endif

/* [yk.kim@lge.com] 2011-01-25, get manual test mode NV */
#ifdef CONFIG_LGE_USB_GADGET_SUPPORT_FACTORY_USB
int msm_get_manual_test_mode(void)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	char output[4];
	int rc= -1;

	Open_check();

	arg.event = LG_FW_MANUAL_TEST_MODE;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = 0;
	arg.input = (char*) NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = 4;

	ret.output = NULL;
	ret.out_len = NULL;

	rc = oem_rapi_client_streaming_function(client, &arg, &ret);
	
/* BEGIN: 0015327 jihoon.lee@lge.com 20110204 */
/* MOD 0015327: [KERNEL] LG RAPI validity check */
	if (rc < 0)
		pr_err("%s error \r\n", __func__);
	else
	{
		rc = lg_rapi_check_validity_and_copy_result((void*)&ret, (char*)output, arg.output_size);
		if(rc == LG_RAPI_INVALID_RESPONSE)
			memset(output,0,4);
		else
			printk(KERN_INFO "MANUAL_TEST_MODE nv : %d\n", GET_INT32(output));
	}
/* END: 0015327 jihoon.lee@lge.com 20110204 */

	//memcpy(output,ret.output,*ret.out_len);

	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);

	return (GET_INT32(output));
}

EXPORT_SYMBOL(msm_get_manual_test_mode);
#endif
#if 1//def LG_FW_SPG_PORT
int msm_get_SPG_PORT_mode(void)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	char output[4];
	int rc= -1;

	Open_check();

	arg.event = LG_FW_SPG_PORT_MODE;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = 0;
	arg.input = (char*) NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = 4;

	ret.output = NULL;
	ret.out_len = NULL;

	rc = oem_rapi_client_streaming_function(client, &arg, &ret);
	
/* BEGIN: 0015327 jihoon.lee@lge.com 20110204 */
/* MOD 0015327: [KERNEL] LG RAPI validity check */
	if (rc < 0)
		pr_err("%s error \r\n", __func__);
	else
	{
		rc = lg_rapi_check_validity_and_copy_result((void*)&ret, (char*)output, arg.output_size);
		if(rc == LG_RAPI_INVALID_RESPONSE)
			memset(output,0,4);
		else
			printk(KERN_INFO "SPG_PORT_MODE nv : %d\n", GET_INT32(output));
	}
/* END: 0015327 jihoon.lee@lge.com 20110204 */

	//memcpy(output,ret.output,*ret.out_len);

	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);

	return (GET_INT32(output));
}

EXPORT_SYMBOL(msm_get_SPG_PORT_mode);
#endif

/* BEGIN: 0016311 jihoon.lee@lge.com 20110217 */
/* ADD 0016311: [POWER OFF] CALL EFS_SYNC */
#ifdef CONFIG_LGE_SUPPORT_RAPI
int remote_rpc_request(uint32_t command)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	char output[4];
	int rc= -1;
	int request_cmd = command;

	Open_check();
	arg.event = LGE_RPC_HANDLE_REQUEST;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = sizeof(request_cmd);
	arg.input = (char*)&request_cmd;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = sizeof(output);

	ret.output = (char*)NULL;
	ret.out_len = 0;

	rc = oem_rapi_client_streaming_function(client,&arg,&ret);
	if (rc < 0)
	{
		pr_err("%s, rapi reqeust failed\r\n", __func__);
	}
	else
	{
		rc = lg_rapi_check_validity_and_copy_result((void*)&ret, (char*)output, arg.output_size);
		if(rc == LG_RAPI_INVALID_RESPONSE)
			memset(output,0,sizeof(output));
		
		switch(command)
		{
#ifdef CONFIG_LGE_SYNC_CMD
			case LGE_SYNC_REQUEST:
				pr_info("%s, sync retry count : %d\r\n", __func__, GET_INT32(output));
				if(rc != LG_RAPI_SUCCESS)
					rc = -1;
				break;
#endif			
			default :
				break;
		}
	}
	
	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);
	
	return rc;
}
#endif /*CONFIG_LGE_SUPPORT_RAPI*/
/* END: 0016311 jihoon.lee@lge.com 20110217 */

#ifdef CONFIG_LGE_USB_GADGET_LLDM_DRIVER
void lldm_sdio_info_set(int lldm_sdio_enable)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int rc= -1;

	Open_check();

	arg.event = LG_FW_LLDM_SDIO_INFO_SET;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = sizeof(int);
	arg.input = (char*) &lldm_sdio_enable;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;  /* alloc memory for response */

	ret.output = (char*)NULL;
	ret.out_len = 0;

	rc = oem_rapi_client_streaming_function(client, &arg, &ret);
	if (rc < 0)
	{
		pr_err("%s, rapi reqeust failed\r\n", __func__);
	}

/* BEGIN: 0014591 jihoon.lee@lge.com 20110122 */
/* MOD 0014591: [LG_RAPI] rpc request heap leakage bug fix */
	// free received buffers if it is not empty
	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);
/* END: 0014591 jihoon.lee@lge.com 2011022 */
	
	return;
}
EXPORT_SYMBOL(lldm_sdio_info_set);

int msm_get_lldm_sdio_info(void)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	char output[4];
	int rc= -1;

	Open_check();

	arg.event = LG_FW_LLDM_SDIO_INFO_GET;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = 0;
	arg.input = (char*) NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = 4;

	ret.output = NULL;
	ret.out_len = NULL;

	rc = oem_rapi_client_streaming_function(client, &arg, &ret);
	
/* BEGIN: 0015327 jihoon.lee@lge.com 20110204 */
/* MOD 0015327: [KERNEL] LG RAPI validity check */
	if (rc < 0)
		pr_err("%s error \r\n", __func__);
	else
	{
		rc = lg_rapi_check_validity_and_copy_result((void*)&ret, (char*)output, arg.output_size);
		if(rc == LG_RAPI_INVALID_RESPONSE)
			memset(output,0,4);
		else
			printk(KERN_INFO "LLDM_SDIO_INFO nv : '%d\n", GET_INT32(output));
	}
/* END: 0015327 jihoon.lee@lge.com 20110204 */

	//memcpy(output,ret.output,*ret.out_len);

	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);

	return (GET_INT32(output));
}
EXPORT_SYMBOL(msm_get_lldm_sdio_info);
#endif

/* LGE_CHANGES_S [jaeho.cho@lge.com] 2010-10-02, charger logo notification to modem */
#ifdef CONFIG_LGE_CHARGING_MODE_INFO
void remote_set_chg_logo_mode(int info)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int ret_val;

	Open_check();

	arg.event = LG_FW_CHG_LOGO_MODE;
	arg.cb_func = NULL;
	arg.handle = (void *)0;
	arg.in_len = sizeof(int);
	arg.input = (char *)&info;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;	//alloc memory for response

	ret.output = NULL;
	ret.out_len = NULL;

	ret_val = oem_rapi_client_streaming_function(client, &arg, &ret);

	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);

	return;
}

EXPORT_SYMBOL(remote_set_chg_logo_mode);
#endif

#ifdef CONFIG_LGE_DLOAD_RESET_BOOT_UP
void firstboot_complete_inform(int info)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int ret_val;
  
	Open_check();

	arg.event = LG_FW_FIRST_BOOT_COMPLETE_EVENT;
	arg.cb_func = NULL;
	arg.handle = (void *)0;
	arg.in_len = sizeof(int);
	arg.input = (char *)&info;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;	//alloc memory for response

	ret.output = NULL;
	ret.out_len = NULL;

	ret_val = oem_rapi_client_streaming_function(client, &arg, &ret);

	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);

	return;
}

EXPORT_SYMBOL(firstboot_complete_inform);
#endif

static void
do_send_SMS_to_arm9(void*	pReq, void* pRsp, int flag, unsigned int rsp_len)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int rc= -1;

	Open_check();
	printk(KERN_INFO "%s %s start\n", __func__, (flag==NORMAL_WORK_FLAG)?"[N]":"[WQ]");

	arg.event = LG_FW_SMS_SEND_EVENT;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	//arg.in_len = sizeof(DIAG_SMS_mode_req_type);
	arg.in_len = 600;
	arg.input = (char*)pReq;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = rsp_len;

	ret.output = NULL;
	ret.out_len = NULL;

	rc= oem_rapi_client_streaming_function(client, &arg, &ret);
	if (rc < 0)
	{
		pr_err("%s, rapi reqeust failed\r\n", __func__);
		((DIAG_SMS_mode_rsp_type*)pRsp)->ret_stat_code = TEST_FAIL_S;
		
	}
	else
	{
		rc = lg_rapi_check_validity_and_copy_result((void*)&ret, (char*)pRsp, arg.output_size);
		if(rc == LG_RAPI_INVALID_RESPONSE)
		{
			((DIAG_SMS_mode_rsp_type*)pRsp)->ret_stat_code = TEST_FAIL_S;
		}
	}

	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);

}

void send_SMS_to_arm9(void*	pReq, void* pRsp, unsigned int rsp_len)
{
	do_send_SMS_to_arm9(pReq, pRsp, NORMAL_WORK_FLAG, rsp_len);
}

//matthew.choi@lge.com prevent efs_sync for recovery mode
#ifdef CONFIG_LGE_RECOVERY_MODE_CHECK
void remote_set_recovery_mode(int info)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int ret_val;

	Open_check();

	arg.event = LG_FW_RECOVERY_MODE;
	arg.cb_func = NULL;
	arg.handle = (void *)0;
	arg.in_len = sizeof(int);
	arg.input = (char *)&info;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;	//alloc memory for response

	ret.output = NULL;
	ret.out_len = NULL;

	ret_val = oem_rapi_client_streaming_function(client, &arg, &ret);

	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);

	return;
}
EXPORT_SYMBOL(remote_set_recovery_mode);
#endif
//Start tao.jin@lge.com LG_FW_CAMERA_CODE added for camcorder current issue 2011-12-23
#if 1
void camera_onoff_inform(int info)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int ret_val;

	Open_check();

	arg.event = LG_FW_CAMERA_MODE;
	arg.cb_func = NULL;
	arg.handle = (void *)0;
	arg.in_len = sizeof(int);
	arg.input = (char *)&info;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;	//alloc memory for response

	ret.output = NULL;
	ret.out_len = NULL;

	ret_val = oem_rapi_client_streaming_function(client, &arg, &ret);

	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);

	return;
}
EXPORT_SYMBOL(camera_onoff_inform);
#endif
//End tao.jin@lge.com LG_FW_CAMERA_CODE added for camcorder current issue 2011-12-23

// matthew.choi@lge.com 120211 LED control from AP [START]
void remote_set_led_on_off(int info)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int ret_val;

	Open_check();

	arg.event = info == 1 ? LG_FW_LED_ON : LG_FW_LED_OFF;
	arg.cb_func = NULL;
	arg.handle = (void *)0;
	arg.in_len = sizeof(int);
	arg.input = (char *)&info;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;	//alloc memory for response

	ret.output = NULL;
	ret.out_len = NULL;

	ret_val = oem_rapi_client_streaming_function(client, &arg, &ret);

	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);

	return;
}
EXPORT_SYMBOL(remote_set_led_on_off);
// matthew.choi@lge.com 120211 LED control from AP [END]

