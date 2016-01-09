/*
 *  TOPPERS ATK2
 *      Toyohashi Open Platform for Embedded Real-Time Systems
 *      Automotive Kernel Version 2
 *
 *  Copyright (C) 2011-2015 by Center for Embedded Computing Systems
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 *  Copyright (C) 2011-2015 by FUJI SOFT INCORPORATED, JAPAN
 *  Copyright (C) 2011-2013 by Spansion LLC, USA
 *  Copyright (C) 2011-2015 by NEC Communication Systems, Ltd., JAPAN
 *  Copyright (C) 2011-2015 by Panasonic Advanced Technology Development Co., Ltd., JAPAN
 *  Copyright (C) 2011-2014 by Renesas Electronics Corporation, JAPAN
 *  Copyright (C) 2011-2015 by Sunny Giken Inc., JAPAN
 *  Copyright (C) 2011-2015 by TOSHIBA CORPORATION, JAPAN
 *  Copyright (C) 2011-2015 by Witz Corporation
 *  Copyright (C) 2014-2015 by AISIN COMCRUISE Co., Ltd., JAPAN
 *  Copyright (C) 2014-2015 by eSOL Co.,Ltd., JAPAN
 *  Copyright (C) 2014-2015 by SCSK Corporation, JAPAN
 *  Copyright (C) 2015 by SUZUKI MOTOR CORPORATION
 *  Copyright (C) 2016 by Fan Wang(parai@foxmail.com), China
 * 
 * The above copyright holders grant permission gratis to use,
 * duplicate, modify, or redistribute (hereafter called use) this
 * software (including the one made by modifying this software),
 * provided that the following four conditions (1) through (4) are
 * satisfied.
 * 
 * (1) When this software is used in the form of source code, the above
 *    copyright notice, this use conditions, and the disclaimer shown
 *    below must be retained in the source code without modification.
 *
 * (2) When this software is redistributed in the forms usable for the
 *    development of other software, such as in library form, the above
 *    copyright notice, this use conditions, and the disclaimer shown
 *    below must be shown without modification in the document provided
 *    with the redistributed software, such as the user manual.
 *
 * (3) When this software is redistributed in the forms unusable for the
 *    development of other software, such as the case when the software
 *    is embedded in a piece of equipment, either of the following two
 *    conditions must be satisfied:
 *
 *  (a) The above copyright notice, this use conditions, and the
 *      disclaimer shown below must be shown without modification in
 *      the document provided with the redistributed software, such as
 *      the user manual.
 *
 *  (b) How the software is to be redistributed must be reported to the
 *      TOPPERS Project according to the procedure described
 *      separately.
 *
 * (4) The above copyright holders and the TOPPERS Project are exempt
 *    from responsibility for any type of damage directly or indirectly
 *    caused from the use of this software and are indemnified by any
 *    users or end users of this software from any and all causes of
 *    action whatsoever.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS." THE ABOVE COPYRIGHT HOLDERS AND
 * THE TOPPERS PROJECT DISCLAIM ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, ITS APPLICABILITY TO A PARTICULAR
 * PURPOSE. IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS AND THE
 * TOPPERS PROJECT BE LIABLE FOR ANY TYPE OF DAMAGE DIRECTLY OR
 * INDIRECTLY CAUSED FROM THE USE OF THIS SOFTWARE.
 *
 *  $Id: counter_manage.c 504 2015-12-24 01:22:56Z witz-itoyo $
 */

/*
 *		カウンタ管理モジュール
 */

#include "kernel_impl.h"
#include "check.h"
#include "counter.h"

/*
 *  トレースログマクロのデフォルト定義
 */
#ifndef LOG_INCCNT_ENTER
#define LOG_INCCNT_ENTER(cntid)
#endif /* LOG_INCCNT_ENTER */

#ifndef LOG_INCCNT_LEAVE
#define LOG_INCCNT_LEAVE(ercd)
#endif /* LOG_INCCNT_LEAVE */

#ifndef LOG_GETCNT_ENTER
#define LOG_GETCNT_ENTER(cntid)
#endif /* LOG_GETCNT_ENTER */

#ifndef LOG_GETCNT_LEAVE
#define LOG_GETCNT_LEAVE(ercd, p_val)
#endif /* LOG_GETCNT_LEAVE */

#ifndef LOG_GETEPS_ENTER
#define LOG_GETEPS_ENTER(cntid, p_val)
#endif /* LOG_GETEPS_ENTER */

#ifndef LOG_GETEPS_LEAVE
#define LOG_GETEPS_LEAVE(ercd, p_val, p_eval)
#endif /* LOG_GETEPS_LEAVE */

/*
 *  カウンタのインクリメント
 */
#ifdef TOPPERS_IncrementCounter

StatusType
IncrementCounter(CounterType CounterID)
{
	StatusType	ercd = E_OK;
	CNTCB		*p_cntcb;
	OSAPCB		*p_osapcb;

	CHECK_DISABLEDINT();
	CHECK_CALLEVEL(CALLEVEL_INCREMENTCOUNTER);
	CHECK_ID(CounterID < tnum_counter);
	CHECK_ID(CounterID >= tnum_hardcounter);

	p_cntcb = get_cntcb(CounterID);
	CHECK_RIGHT(p_cntcb->p_cntinib->acsbtmp);

	p_osapcb = p_cntcb->p_cntinib->p_osapcb;
	x_nested_lock_os_int();

	/* カウンタ所属のOSAPの状態をチェック */
	D_CHECK_ACCESS((p_osapcb->osap_stat == APPLICATION_ACCESSIBLE) ||
				   ((p_osapcb->osap_stat == APPLICATION_RESTARTING) &&
					(p_osapcb == p_runosap)));

	/*
	 *  カウンタのインクリメント
	 *  エラーの場合はincr_counter_processでエラーフック呼び出しているので，
	 *  ここでは，エラーフックを呼び出さない
	 */
	ercd = incr_counter_process(p_cntcb, CounterID);

  d_exit_no_errorhook:
	x_nested_unlock_os_int();
  exit_no_errorhook:
	return(ercd);

#ifdef CFG_USE_ERRORHOOK
  exit_errorhook:
	x_nested_lock_os_int();
  d_exit_errorhook:
#ifdef CFG_USE_PARAMETERACCESS
	_errorhook_par1.cntid = CounterID;
#endif /* CFG_USE_PARAMETERACCESS */
	call_errorhook(ercd, OSServiceId_IncrementCounter);
	goto d_exit_no_errorhook;
#endif /* CFG_USE_ERRORHOOK */
}

#endif /* TOPPERS_IncrementCounter */

/*
 *  カウンタ値の参照
 */
#ifdef TOPPERS_GetCounterValue

StatusType
GetCounterValue(CounterType CounterID, TickRefType Value)
{
	StatusType	ercd = E_OK;
	CNTCB		*p_cntcb;
	TickType	curval;
	OSAPCB		*p_osapcb;

	LOG_GETCNT_ENTER(CounterID);
	CHECK_DISABLEDINT();
	CHECK_CALLEVEL(CALLEVEL_GETCOUNTERVALUE);
	CHECK_ID(CounterID < tnum_counter);
	CHECK_PARAM_POINTER(Value);
	CHECK_MEM_WRITE(Value, TickType);
	p_cntcb = get_cntcb(CounterID);
	CHECK_RIGHT(p_cntcb->p_cntinib->acsbtmp);

	/*
	 *  内部処理のため，コンフィギュレーション設定値の２倍＋１までカウント
	 *  アップするのでカウンタ値が設定値よりも大きい場合は設定値を減算する
	 *
	 *  *Value を直接操作してもよいが,局所変数がレジスタに割当てられること
	 *  による速度を期待している
	 */
	p_osapcb = p_cntcb->p_cntinib->p_osapcb;
	x_nested_lock_os_int();

	/* カウンタ所属のOSAPの状態をチェック */
	D_CHECK_ACCESS((p_osapcb->osap_stat == APPLICATION_ACCESSIBLE) ||
				   ((p_osapcb->osap_stat == APPLICATION_RESTARTING) &&
					(p_osapcb == p_runosap)));

	curval = get_curval(p_cntcb, CounterID);
	x_nested_unlock_os_int();

	if (curval > p_cntcb->p_cntinib->maxval) {
		curval -= (p_cntcb->p_cntinib->maxval + 1U);
	}
	*Value = curval;

  exit_no_errorhook:
	LOG_GETCNT_LEAVE(ercd, Value);
	return(ercd);

#ifdef CFG_USE_ERRORHOOK
  exit_errorhook:
	x_nested_lock_os_int();
  d_exit_errorhook:
#ifdef CFG_USE_PARAMETERACCESS
	_errorhook_par1.cntid = CounterID;
	_errorhook_par2.p_val = Value;
#endif /* CFG_USE_PARAMETERACCESS */
	call_errorhook(ercd, OSServiceId_GetCounterValue);
#endif /* CFG_USE_ERRORHOOK */
  d_exit_no_errorhook:
	x_nested_unlock_os_int();
	goto exit_no_errorhook;
}

#endif /* TOPPERS_GetCounterValue */

/*
 *  経過カウンタ値の参照
 */
#ifdef TOPPERS_GetElapsedValue

StatusType
GetElapsedValue(CounterType CounterID, TickRefType Value, TickRefType ElapsedValue)
{
	StatusType	ercd = E_OK;
	CNTCB		*p_cntcb;
	TickType	curval;
	OSAPCB		*p_osapcb;

	LOG_GETEPS_ENTER(CounterID, Value);
	CHECK_DISABLEDINT();
	CHECK_CALLEVEL(CALLEVEL_GETELAPSEDVALUE);
	CHECK_ID(CounterID < tnum_counter);
	CHECK_PARAM_POINTER(Value);
	CHECK_PARAM_POINTER(ElapsedValue);
	CHECK_MEM_RW(Value,  TickType);
	CHECK_MEM_WRITE(ElapsedValue, TickType);
	p_cntcb = get_cntcb(CounterID);
	CHECK_RIGHT(p_cntcb->p_cntinib->acsbtmp);

	CHECK_VALUE(*Value <= p_cntcb->p_cntinib->maxval);

	/*
	 *  内部処理のため，コンフィギュレーション設定値の２倍＋１までカウント
	 *  アップするのでカウンタ値が設定値よりも大きい場合は設定値を減算する
	 */
	p_osapcb = p_cntcb->p_cntinib->p_osapcb;
	x_nested_lock_os_int();

	/* カウンタ所属のOSAPの状態をチェック */
	D_CHECK_ACCESS((p_osapcb->osap_stat == APPLICATION_ACCESSIBLE) ||
				   ((p_osapcb->osap_stat == APPLICATION_RESTARTING) &&
					(p_osapcb == p_runosap)));

	curval = get_curval(p_cntcb, CounterID);
	x_nested_unlock_os_int();

	if (curval > p_cntcb->p_cntinib->maxval) {
		curval -= (p_cntcb->p_cntinib->maxval + 1U);
	}
	*ElapsedValue = diff_tick(curval, *Value, p_cntcb->p_cntinib->maxval);
	*Value = curval;

  exit_no_errorhook:
	LOG_GETEPS_LEAVE(ercd, Value, ElapsedValue);
	return(ercd);

#ifdef CFG_USE_ERRORHOOK
  exit_errorhook:
	x_nested_lock_os_int();
  d_exit_errorhook:
#ifdef CFG_USE_PARAMETERACCESS
	_errorhook_par1.cntid = CounterID;
	_errorhook_par2.p_val = Value;
	_errorhook_par3.p_eval = ElapsedValue;
#endif /* CFG_USE_PARAMETERACCESS */
	call_errorhook(ercd, OSServiceId_GetElapsedValue);
#endif /* CFG_USE_ERRORHOOK */
  d_exit_no_errorhook:
	x_nested_unlock_os_int();
	goto exit_no_errorhook;
}

#endif /* TOPPERS_GetElapsedValue */

/*
 *  ハードウェアカウンタ満了処理
 *
 *  割込みルーチンより実行される
 */
#ifdef TOPPERS_notify_hardware_counter

void
notify_hardware_counter(CounterType cntid)
{
	CNTCB *p_cntcb;

	p_cntcb = get_cntcb(cntid);

	/* カウンタ満了処理中はOS割込みを禁止 */
	x_nested_lock_os_int();

	/*
	 *  ハードウェアカウンタに対応するC2ISRが起動した際に，
	 *  割込み要求のクリア処理を実行する
	 */
	(hwcntinib_table[cntid].intclear)();

	expire_process(p_cntcb, cntid);

	x_nested_unlock_os_int();
}

#endif /* TOPPERS_notify_hardware_counter */

/*
 *  カウンタのインクリメント
 *
 *  条件：割込み禁止状態で呼ばれる
 */
#ifdef TOPPERS_incr_counter_process

StatusType
incr_counter_process(CNTCB *p_cntcb, CounterType CounterID)
{
	StatusType	ercd = E_OK;
	TickType	newval;

	LOG_INCCNT_ENTER(CounterID);

	/*
	 *  カウンタが操作中(IncrementCounterのネスト）の場合エラー
	 *  ※独自仕様
	 */
	D_CHECK_STATE(p_cntcb->cstat == CS_NULL);

	p_cntcb->cstat = CS_DOING;

	newval = add_tick(p_cntcb->curval, 1U, p_cntcb->p_cntinib->maxval2);

	p_cntcb->curval = newval;

	if (p_runisr == NULL) {
		p_cntcb->p_prevcntcb = p_runtsk->p_lastcntcb;
		p_runtsk->p_lastcntcb = p_cntcb;
	}

	expire_process(p_cntcb, CounterID);

	if (p_runisr == NULL) {
		p_runtsk->p_lastcntcb = p_cntcb->p_prevcntcb;
	}

	p_cntcb->cstat = CS_NULL;

  d_exit_no_errorhook:
	LOG_INCCNT_LEAVE(ercd);
	return(ercd);

#ifdef CFG_USE_ERRORHOOK
  d_exit_errorhook:
#ifdef CFG_USE_PARAMETERACCESS
	_errorhook_par1.cntid = CounterID;
#endif /* CFG_USE_PARAMETERACCESS */
	call_errorhook(ercd, OSServiceId_IncrementCounter);
	goto d_exit_no_errorhook;
#endif /* CFG_USE_ERRORHOOK */
}

#endif /* TOPPERS_incr_counter_process */

/*
 *  アラーム満了によるカウンタのインクリメント
 *
 *  条件：割込み禁止状態で呼ばれる
 */
#ifdef TOPPERS_incr_counter_action

StatusType
incr_counter_action(OSAPCB *p_expire_osapcb, CounterType CounterID)
{
	StatusType	ercd = E_OK;
	CNTCB		*p_cntcb;
	OSAPCB		*p_osapcb;

	p_cntcb = get_cntcb(CounterID);
	p_osapcb = p_cntcb->p_cntinib->p_osapcb;
	/* 満了点所属のOSAP及びインクリメントカウンタ所属のOSAPの状態をチェック */
	D_CHECK_ACCESS((p_osapcb->osap_stat == APPLICATION_ACCESSIBLE) || (p_expire_osapcb == p_osapcb));

	ercd = incr_counter_process(p_cntcb, CounterID);

  d_exit_no_errorhook:
	return(ercd);

#ifdef CFG_USE_ERRORHOOK
  d_exit_errorhook:
#ifdef CFG_USE_PARAMETERACCESS
	_errorhook_par1.cntid = CounterID;
#endif /* CFG_USE_PARAMETERACCESS */
	call_errorhook(ercd, OSServiceId_IncrementCounter);
	goto d_exit_no_errorhook;
#endif /* CFG_USE_ERRORHOOK */
}

#endif /* TOPPERS_incr_counter_action */
