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
 *  $Id: resource.c 504 2015-12-24 01:22:56Z witz-itoyo $
 */

/*
 *		リソース管理モジュール
 */

#include "kernel_impl.h"
#include "check.h"
#include "task.h"
#include "interrupt.h"
#include "resource.h"
#include "timingprotection.h"

/*
 *  トレースログマクロのデフォルト定義
 */
#ifndef LOG_GETRES_ENTER
#define LOG_GETRES_ENTER(resid)
#endif /* LOG_GETRES_ENTER */

#ifndef LOG_GETRES_LEAVE
#define LOG_GETRES_LEAVE(ercd)
#endif /* LOG_GETRES_LEAVE */

#ifndef LOG_RELRES_ENTER
#define LOG_RELRES_ENTER(resid)
#endif /* LOG_RELRES_ENTER */

#ifndef LOG_RELRES_LEAVE
#define LOG_RELRES_LEAVE(ercd)
#endif /* LOG_RELRES_LEAVE */

/*
 *  リソース管理機能の初期化
 */
#ifdef TOPPERS_resource_initialize

void
resource_initialize(void)
{
	ResourceType	i;
	RESCB			*p_rescb;

	for (i = 0U; i < tnum_stdresource; i++) {
		p_rescb = &(rescb_table[i]);
		p_rescb->p_resinib = &(resinib_table[i]);
		p_rescb->lockflg = FALSE;
	}
}

#endif /* TOPPERS_resource_initialize */

/*
 *  リソースの獲得
 */
#ifdef TOPPERS_GetResource

StatusType
GetResource(ResourceType ResID)
{
	StatusType		ercd = E_OK;
	PriorityType	ceilpri, curpri;
	RESCB			*p_rescb;
	TickType		tp_timer;

	LOG_GETRES_ENTER(ResID);
	CHECK_DISABLEDINT();
	CHECK_CALLEVEL(CALLEVEL_GETRESOURCE);
	CHECK_ID(ResID < tnum_stdresource);
	p_rescb = get_rescb(ResID);
	CHECK_RIGHT(p_rescb->p_resinib->acsbtmp);

	ceilpri = p_rescb->p_resinib->ceilpri;
	if (callevel_stat == TCL_TASK) {
		CHECK_ACCESS(p_runtsk->p_tinib->inipri >= ceilpri);

		x_nested_lock_os_int();
		D_CHECK_ACCESS(p_rescb->lockflg == FALSE);
		curpri = p_runtsk->curpri;
		p_rescb->prevpri = curpri;
		p_rescb->lockflg = TRUE;
		p_rescb->p_prevrescb = p_runtsk->p_lastrescb;
		p_runtsk->p_lastrescb = p_rescb;

		if ((p_rescb->p_resinib->res_lockbudget != 0U) && ((p_runtsk->p_tinib->monitoring & BITMAP_RESLOCK) != 0U)) {
			/*
			 *  リソース監視処理
			 */
			/* 一つ前の監視項目に現在の監視項目を保存 */
			p_rescb->res_saved_watchtype = p_runtsk->watchtype;

			if (p_runtsk->watchtype == NON_MONITORING) {
				/*
				 *  現在の監視項目がない
				 */

				/* 現在の監視項目にRESOURCE_MONITORINGを保存 */
				p_runtsk->watchtype = RESOURCE_MONITORING;
				/* リソース占有時間バジェットでタイマを開始 */
				tp_start_timer(p_rescb->p_resinib->res_lockbudget);
			}
			else {
				/*
				 *  現在の監視項目がタスクかリソース
				 */
				/* 残り実行時間バジェットを取得 */
				tp_timer = target_tp_get_remaining_ticks();
				if (tp_timer > p_rescb->p_resinib->res_lockbudget) {
					/*
					 *  タイマ切替処理
					 */
					/* 現在の監視項目にRESOURCE_MONITORINGを保存 */
					p_runtsk->watchtype = RESOURCE_MONITORING;
					/* 残り実行時間バジェットとリソース占有時間バジェットの差を保存 */
					p_rescb->res_difftime = tp_timer - p_rescb->p_resinib->res_lockbudget;
					/* リソース占有時間バジェットでタイマを開始 */
					tp_start_timer(p_rescb->p_resinib->res_lockbudget);
				}
			}
		}

		if (ceilpri < curpri) {
			p_runtsk->curpri = ceilpri;
			if (ceilpri <= TPRI_MINISR) {
				x_set_ipm(ceilpri);
			}
		}
	}
	else {
		CHECK_ACCESS(GET_INTPRI(p_runisr) >= ceilpri);

		x_nested_lock_os_int();
		D_CHECK_ACCESS(p_rescb->lockflg == FALSE);
		curpri = x_get_ipm();
		p_rescb->prevpri = curpri;
		p_rescb->lockflg = TRUE;
		p_rescb->p_prevrescb = p_runisr->p_lastrescb;
		p_runisr->p_lastrescb = p_rescb;
		if (ceilpri < curpri) {
			x_set_ipm(ceilpri);
		}
	}

  d_exit_no_errorhook:
	x_nested_unlock_os_int();
  exit_no_errorhook:
	LOG_GETRES_LEAVE(ercd);
	return(ercd);

#ifdef CFG_USE_ERRORHOOK
  exit_errorhook:
	x_nested_lock_os_int();
  d_exit_errorhook:
#ifdef CFG_USE_PARAMETERACCESS
	_errorhook_par1.resid = ResID;
#endif /* CFG_USE_PARAMETERACCESS */
	call_errorhook(ercd, OSServiceId_GetResource);
	goto d_exit_no_errorhook;
#endif /* CFG_USE_ERRORHOOK */
}

#endif /* TOPPERS_GetResource */

/*
 *  リソースの返却
 */
#ifdef TOPPERS_ReleaseResource

StatusType
ReleaseResource(ResourceType ResID)
{
	StatusType	ercd = E_OK;
	RESCB		*p_rescb;
	TickType	tp_timer;

	LOG_RELRES_ENTER(ResID);
	CHECK_DISABLEDINT();
	CHECK_CALLEVEL(CALLEVEL_RELEASERESOURCE);
	CHECK_ID(ResID < tnum_stdresource);
	p_rescb = get_rescb(ResID);
	CHECK_RIGHT(p_rescb->p_resinib->acsbtmp);

	if (callevel_stat == TCL_TASK) {
		CHECK_NOFUNC(p_runtsk->p_lastrescb == p_rescb);

		x_nested_lock_os_int();
		if (p_rescb->prevpri <= TPRI_MINISR) {
			x_set_ipm(p_rescb->prevpri);
		}
		else {
			if (p_runtsk->curpri <= TPRI_MINISR) {
				x_set_ipm((PriorityType) TIPM_ENAALL);
			}
		}
		p_runtsk->curpri = p_rescb->prevpri;
		p_runtsk->p_lastrescb = p_rescb->p_prevrescb;

		if ((p_rescb->p_resinib->res_lockbudget != 0U) && ((p_runtsk->p_tinib->monitoring & BITMAP_RESLOCK) != 0U)) {
			/*
			 *  リソース監視処理
			 */
			/* 現在の監視項目に一つ前の監視項目を保存 */
			p_runtsk->watchtype = p_rescb->res_saved_watchtype;

			/*
			 *  リソース監視処理
			 */
			if (p_rescb->res_saved_watchtype == NON_MONITORING) {
				/*
				 *  一つ前の監視項目がない
				 */
				/* 残り実行時間バジェットを取得し，停止する*/
				tp_timer = tp_stop_timer();
			}
			else {
				/*
				 *  一つ前の監視項目がタスクかリソース
				 */
				if (p_rescb->res_difftime != 0U) {
					/*
					 *  タイマ切替処理
					 */
					/* 残り実行時間バジェットを取得し，停止する*/
					tp_timer = tp_stop_timer();
					/* GetResource時に保存したres_difftimeとタイマ値を一つ前の監視項目の残り時間を復帰させ，タイマをスタート */
					tp_start_timer(p_rescb->res_difftime + tp_timer);
					/* p_rescb->difftimeの初期化 */
					p_rescb->res_difftime = 0U;
				}
			}
		}

		p_rescb->lockflg = FALSE;
		if (p_runtsk->curpri > nextpri) {
			preempt();
			dispatch();
		}
	}
	else {
		CHECK_NOFUNC(p_runisr->p_lastrescb == p_rescb);

		x_nested_lock_os_int();
		x_set_ipm(p_rescb->prevpri);
		p_runisr->p_lastrescb = p_rescb->p_prevrescb;
		p_rescb->lockflg = FALSE;
	}

  d_exit_no_errorhook:
	x_nested_unlock_os_int();
  exit_no_errorhook:
	LOG_RELRES_LEAVE(ercd);
	return(ercd);

#ifdef CFG_USE_ERRORHOOK
  exit_errorhook:
	x_nested_lock_os_int();
#ifdef CFG_USE_PARAMETERACCESS
	_errorhook_par1.resid = ResID;
#endif /* CFG_USE_PARAMETERACCESS */
	call_errorhook(ercd, OSServiceId_ReleaseResource);
	goto d_exit_no_errorhook;
#endif /* CFG_USE_ERRORHOOK */
}

#endif /* TOPPERS_ReleaseResource */
