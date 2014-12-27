/*
 * Copyright (c) 1996-2014, Paul Mattes.
 * Copyright (c) 1995, Dick Altenbern.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Dick Altenbern nor the names of
 *       their contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES AND DICK ALTENBERN "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PAUL MATTES OR DICK ALTENBERN BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 *	ft_gui.c
 *		IND$FILE file transfer dialogs.
 */

#include "globals.h"

#include <assert.h>

#include <X11/StringDefs.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Form.h>
#include <X11/Shell.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/TextSrc.h>
#include <X11/Xaw/TextSink.h>
#include <X11/Xaw/AsciiSrc.h>
#include <X11/Xaw/AsciiSink.h>
#include <errno.h>

#include "appres.h"
#include "dialogc.h"
#include "ftc.h"
#include "ft_dftc.h"
#include "ft_private.h"
#include "kybdc.h"
#include "menubarc.h"
#include "objects.h"
#include "popupsc.h"
#include "utilc.h"
#include "varbufc.h"

#include "ft_guic.h"

/* Macros. */
#define FILE_WIDTH	300	/* width of file name widgets */
#define MARGIN		3	/* distance from margins to widgets */
#define CLOSE_VGAP	0	/* distance between paired toggles */
#define FAR_VGAP	10	/* distance between single toggles and groups */
#define BUTTON_GAP	5	/* horizontal distance between buttons */
#define COLUMN_GAP	40	/* distance between columns */

/* Globals. */

/* Statics. */
static Widget ft_dialog, ft_shell, local_file, host_file;
static Widget lrecl_widget, blksize_widget;
static Widget primspace_widget, secspace_widget;
static Widget send_toggle, receive_toggle;
static Widget vm_toggle, tso_toggle, cics_toggle;
static Widget ascii_toggle, binary_toggle;
static Widget cr_widget;
static Widget remap_widget;
static Widget buffersize_widget;

static Boolean host_is_tso = True;	/* Booleans used by dialog */
static Boolean host_is_tso_or_vm = True;/*  sensitivity logic */
static host_type_t s_tso = HT_TSO;	/* Values used by toggle callbacks. */
static host_type_t s_vm = HT_VM;
static host_type_t s_cics = HT_CICS;
static Widget recfm_options[5];
static Widget units_options[5];
static struct toggle_list recfm_toggles = { recfm_options };
static struct toggle_list units_toggles = { units_options };

static Boolean recfm_default = True;
static enum recfm r_default_recfm = DEFAULT_RECFM;
static enum recfm r_fixed = RECFM_FIXED;
static enum recfm r_variable = RECFM_VARIABLE;
static enum recfm r_undefined = RECFM_UNDEFINED;

static Boolean units_default = True;
static enum units u_default_units = DEFAULT_UNITS;
static enum units u_tracks = TRACKS;
static enum units u_cylinders = CYLINDERS;
static enum units u_avblock = AVBLOCK;

static sr_t *ft_sr = NULL;

static Widget progress_shell, from_file, to_file;
static Widget ft_status, waiting, aborting;
static String status_string;

static Widget overwrite_shell;

static void ft_cancel(Widget w, XtPointer client_data, XtPointer call_data);
static void ft_popup_callback(Widget w, XtPointer client_data,
    XtPointer call_data);
static void ft_popup_init(void);
static Boolean ft_start(void);
static void ft_start_callback(Widget w, XtPointer call_parms,
    XtPointer call_data);
static void overwrite_cancel_callback(Widget w, XtPointer client_data,
    XtPointer call_data);
static void overwrite_okay_callback(Widget w, XtPointer client_data,
    XtPointer call_data);
static void overwrite_popdown(Widget w, XtPointer client_data,
    XtPointer call_data);
static void overwrite_popup_init(void);
static void popup_overwrite(void);
static void popup_progress(void);
static void progress_cancel_callback(Widget w, XtPointer client_data,
    XtPointer call_data);
static void progress_popup_callback(Widget w, XtPointer client_data,
    XtPointer call_data);
static void progress_popup_init(void);
static void recfm_callback(Widget w, XtPointer user_data, XtPointer call_data);
static void toggle_append(Widget w, XtPointer client_data, XtPointer call_data);
static void toggle_ascii(Widget w, XtPointer client_data, XtPointer call_data);
static void toggle_cr(Widget w, XtPointer client_data, XtPointer call_data);
static void toggle_remap(Widget w, XtPointer client_data, XtPointer call_data);
static void toggle_receive(Widget w, XtPointer client_data,
    XtPointer call_data);
static void toggle_host_type(Widget w, XtPointer client_data,
    XtPointer call_data);
static void units_callback(Widget w, XtPointer user_data, XtPointer call_data);

/* "File Transfer" dialog. */

/*
 * Pop up the "Transfer" menu.
 * Called back from the "File Transfer" option on the File menu.
 */
void
popup_ft(Widget w _is_unused, XtPointer call_parms _is_unused,
	XtPointer call_data _is_unused)
{
    /* Initialize it. */
    if (ft_shell == NULL) {
	ft_popup_init();
    }

    /* Pop it up. */
    dialog_set(&ft_sr, ft_dialog);
    popup_popup(ft_shell, XtGrabNone);
}

/* Initialize the transfer pop-up. */
static void
ft_popup_init(void)
{
    Widget w;
    Widget cancel_button;
    Widget local_label, host_label;
    Widget append_widget;
    Widget lrecl_label, blksize_label, primspace_label, secspace_label;
    Widget h_ref = NULL;
    Dimension d1;
    Dimension maxw = 0;
    Widget recfm_label, units_label;
    Widget buffersize_label;
    Widget start_button;
    Widget spacer_toggle;
    char *s;

    /* Prep the dialog functions. */
    dialog_set(&ft_sr, ft_dialog);

    /* Create the menu shell. */
    ft_shell = XtVaCreatePopupShell(
	    "ftPopup", transientShellWidgetClass, toplevel,
	    NULL);
    XtAddCallback(ft_shell, XtNpopupCallback, place_popup, (XtPointer)CenterP);
    XtAddCallback(ft_shell, XtNpopupCallback, ft_popup_callback, NULL);

    /* Create the form within the shell. */
    ft_dialog = XtVaCreateManagedWidget(
	    ObjDialog, formWidgetClass, ft_shell,
	    NULL);

    /* Create the file name widgets. */
    local_label = XtVaCreateManagedWidget(
	    "local", labelWidgetClass, ft_dialog,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    local_file = XtVaCreateManagedWidget(
	    "value", asciiTextWidgetClass, ft_dialog,
	    XtNeditType, XawtextEdit,
	    XtNwidth, FILE_WIDTH,
	    XtNvertDistance, FAR_VGAP,
	    XtNfromHoriz, local_label,
	    XtNhorizDistance, 0,
	    NULL);
    dialog_match_dimension(local_label, local_file, XtNheight);
    w = XawTextGetSource(local_file);
    if (w == NULL) {
	XtWarning("Cannot find text source in dialog");
    } else {
	XtAddCallback(w, XtNcallback, dialog_text_callback,
		(XtPointer)&t_unixfile);
    }
    dialog_register_sensitivity(local_file,
	    NULL, False,
	    NULL, False,
	    NULL, False);

    host_label = XtVaCreateManagedWidget(
	    "host", labelWidgetClass, ft_dialog,
	    XtNfromVert, local_label,
	    XtNvertDistance, 3,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    host_file = XtVaCreateManagedWidget(
	    "value", asciiTextWidgetClass, ft_dialog,
	    XtNeditType, XawtextEdit,
	    XtNwidth, FILE_WIDTH,
	    XtNdisplayCaret, False,
	    XtNfromVert, local_label,
	    XtNvertDistance, 3,
	    XtNfromHoriz, host_label,
	    XtNhorizDistance, 0,
	    NULL);
    dialog_match_dimension(host_label, host_file, XtNheight);
    dialog_match_dimension(local_label, host_label, XtNwidth);
    w = XawTextGetSource(host_file);
    if (w == NULL) {
	XtWarning("Cannot find text source in dialog");
    } else {
	XtAddCallback(w, XtNcallback, dialog_text_callback,
		(XtPointer)&t_hostfile);
    }
    dialog_register_sensitivity(host_file,
	    NULL, False,
	    NULL, False,
	    NULL, False);

    /* Create the left column. */

    /* Create send/receive toggles. */
    send_toggle = XtVaCreateManagedWidget(
	    "send", commandWidgetClass, ft_dialog,
	    XtNfromVert, host_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(send_toggle, ft_private.receive_flag? no_diamond:
							      diamond);
    XtAddCallback(send_toggle, XtNcallback, toggle_receive,
	    (XtPointer)&s_false);
    receive_toggle = XtVaCreateManagedWidget(
	    "receive", commandWidgetClass, ft_dialog,
	    XtNfromVert, send_toggle,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(receive_toggle, ft_private.receive_flag? diamond:
								 no_diamond);
    XtAddCallback(receive_toggle, XtNcallback, toggle_receive,
	    (XtPointer)&s_true);
    spacer_toggle = XtVaCreateManagedWidget(
	    "empty", labelWidgetClass, ft_dialog,
	    XtNfromVert, receive_toggle,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    XtNlabel, "",
	    NULL);

    /* Create ASCII/binary toggles. */
    ascii_toggle = XtVaCreateManagedWidget(
	    "ascii", commandWidgetClass, ft_dialog,
	    XtNfromVert, spacer_toggle,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(ascii_toggle, ascii_flag? diamond: no_diamond);
    XtAddCallback(ascii_toggle, XtNcallback, toggle_ascii, (XtPointer)&s_true);
    binary_toggle = XtVaCreateManagedWidget(
	    "binary", commandWidgetClass, ft_dialog,
	    XtNfromVert, ascii_toggle,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(binary_toggle, ascii_flag? no_diamond: diamond);
    XtAddCallback(binary_toggle, XtNcallback, toggle_ascii,
	    (XtPointer)&s_false);

    /* Create append toggle. */
    append_widget = XtVaCreateManagedWidget(
	    "append", commandWidgetClass, ft_dialog,
	    XtNfromVert, binary_toggle,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(append_widget, ft_private.append_flag? dot: no_dot);
    XtAddCallback(append_widget, XtNcallback, toggle_append, NULL);

    /* Set up the recfm group. */
    recfm_label = XtVaCreateManagedWidget(
	    "file", labelWidgetClass, ft_dialog,
	    XtNfromVert, append_widget,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_register_sensitivity(recfm_label,
	    &ft_private.receive_flag, False,
	    &host_is_tso_or_vm, True,
	    NULL, False);

    recfm_options[0] = XtVaCreateManagedWidget(
	    "recfmDefault", commandWidgetClass, ft_dialog,
	    XtNfromVert, recfm_label,
	    XtNvertDistance, 3,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(recfm_options[0],
	    (ft_private.recfm == DEFAULT_RECFM)? diamond: no_diamond);
    XtAddCallback(recfm_options[0], XtNcallback, recfm_callback,
	    (XtPointer)&r_default_recfm);
    dialog_register_sensitivity(recfm_options[0],
	    &ft_private.receive_flag, False,
	    &host_is_tso_or_vm, True,
	    NULL, False);

    recfm_options[1] = XtVaCreateManagedWidget(
	    "fixed", commandWidgetClass, ft_dialog,
	    XtNfromVert, recfm_options[0],
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(recfm_options[1],
	    (ft_private.recfm == RECFM_FIXED)? diamond: no_diamond);
    XtAddCallback(recfm_options[1], XtNcallback, recfm_callback,
	    (XtPointer)&r_fixed);
    dialog_register_sensitivity(recfm_options[1],
	    &ft_private.receive_flag, False,
	    &host_is_tso_or_vm, True,
	    NULL, False);

    recfm_options[2] = XtVaCreateManagedWidget(
	    "variable", commandWidgetClass, ft_dialog,
	    XtNfromVert, recfm_options[1],
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(recfm_options[2],
	    (ft_private.recfm == RECFM_VARIABLE)? diamond: no_diamond);
    XtAddCallback(recfm_options[2], XtNcallback, recfm_callback,
	    (XtPointer)&r_variable);
    dialog_register_sensitivity(recfm_options[2],
	    &ft_private.receive_flag, False,
	    &host_is_tso_or_vm, True,
	    NULL, False);

    recfm_options[3] = XtVaCreateManagedWidget(
	    "undefined", commandWidgetClass, ft_dialog,
	    XtNfromVert, recfm_options[2],
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(recfm_options[3],
	    (ft_private.recfm == RECFM_UNDEFINED)? diamond: no_diamond);
    XtAddCallback(recfm_options[3], XtNcallback, recfm_callback,
	    (XtPointer)&r_undefined);
    dialog_register_sensitivity(recfm_options[3],
	    &ft_private.receive_flag, False,
	    &host_is_tso, True,
	    NULL, False);

    lrecl_label = XtVaCreateManagedWidget(
	    "lrecl", labelWidgetClass, ft_dialog,
	    XtNfromVert, recfm_options[3],
	    XtNvertDistance, 3,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_register_sensitivity(lrecl_label,
	    &ft_private.receive_flag, False,
	    &recfm_default, False,
	    &host_is_tso_or_vm, True);
    lrecl_widget = XtVaCreateManagedWidget(
	    "value", asciiTextWidgetClass, ft_dialog,
	    XtNfromVert, recfm_options[3],
	    XtNvertDistance, 3,
	    XtNfromHoriz, lrecl_label,
	    XtNhorizDistance, MARGIN,
	    XtNwidth, 100,
	    XtNeditType, XawtextEdit,
	    XtNdisplayCaret, False,
	    NULL);
    dialog_match_dimension(lrecl_label, lrecl_widget, XtNheight);
    w = XawTextGetSource(lrecl_widget);
    if (w == NULL) {
	XtWarning("Cannot find text source in dialog");
    } else {
	XtAddCallback(w, XtNcallback, dialog_text_callback,
		(XtPointer)&t_numeric);
    }
    dialog_register_sensitivity(lrecl_widget,
	    &ft_private.receive_flag, False,
	    &recfm_default, False,
	    &host_is_tso_or_vm, True);

    blksize_label = XtVaCreateManagedWidget(
	    "blksize", labelWidgetClass, ft_dialog,
	    XtNfromVert, lrecl_widget,
	    XtNvertDistance, 3,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    dialog_match_dimension(blksize_label, lrecl_label, XtNwidth);
    dialog_register_sensitivity(blksize_label,
	    &ft_private.receive_flag, False,
	    &recfm_default, False,
	    &host_is_tso_or_vm, True);
    blksize_widget = XtVaCreateManagedWidget(
	    "value", asciiTextWidgetClass, ft_dialog,
	    XtNfromVert, lrecl_widget,
	    XtNvertDistance, 3,
	    XtNfromHoriz, blksize_label,
	    XtNhorizDistance, MARGIN,
	    XtNwidth, 100,
	    XtNeditType, XawtextEdit,
	    XtNdisplayCaret, False,
	    NULL);
    dialog_match_dimension(blksize_label, blksize_widget, XtNheight);
    w = XawTextGetSource(blksize_widget);
    if (w == NULL) {
	XtWarning("Cannot find text source in dialog");
    } else {
	XtAddCallback(w, XtNcallback, dialog_text_callback,
		(XtPointer)&t_numeric);
    }
    dialog_register_sensitivity(blksize_widget,
	    &ft_private.receive_flag, False,
	    &recfm_default, False,
	    &host_is_tso_or_vm, True);

    /* Find the widest widget in the left column. */
    XtVaGetValues(send_toggle, XtNwidth, &maxw, NULL);
    h_ref = send_toggle;
#define REMAX(w) { \
	XtVaGetValues((w), XtNwidth, &d1, NULL); \
	if (d1 > maxw) { \
	    maxw = d1; \
	    h_ref = (w); \
	} \
    }
    REMAX(receive_toggle);
    REMAX(ascii_toggle);
    REMAX(binary_toggle);
    REMAX(append_widget);
#undef REMAX

    /* Create the right column buttons. */

    /* Create VM/TSO/CICS toggles. */
    vm_toggle = XtVaCreateManagedWidget(
	    "vm", commandWidgetClass, ft_dialog,
	    XtNfromVert, host_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(vm_toggle,
	    (ft_private.host_type == HT_VM)? diamond: no_diamond);
    XtAddCallback(vm_toggle, XtNcallback, toggle_host_type,
	    (XtPointer)&s_vm);
    tso_toggle =  XtVaCreateManagedWidget(
	    "tso", commandWidgetClass, ft_dialog,
	    XtNfromVert, vm_toggle,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(tso_toggle,
	    (ft_private.host_type == HT_TSO)? diamond : no_diamond);
    XtAddCallback(tso_toggle, XtNcallback, toggle_host_type,
	    (XtPointer)&s_tso);
    cics_toggle =  XtVaCreateManagedWidget(
	    "cics", commandWidgetClass, ft_dialog,
	    XtNfromVert, tso_toggle,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(cics_toggle,
	    (ft_private.host_type == HT_CICS)? diamond : no_diamond);
    XtAddCallback(cics_toggle, XtNcallback, toggle_host_type,
	    (XtPointer)&s_cics);

    /* Create CR toggle. */
    cr_widget = XtVaCreateManagedWidget(
	    "cr", commandWidgetClass, ft_dialog,
	    XtNfromVert, cics_toggle,
	    XtNvertDistance, FAR_VGAP,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(cr_widget, cr_flag? dot: no_dot);
    XtAddCallback(cr_widget, XtNcallback, toggle_cr, 0);
    dialog_register_sensitivity(cr_widget,
	    NULL, False,
	    NULL, False,
	    NULL, False);

    /* Create remap toggle. */
    remap_widget = XtVaCreateManagedWidget(
	    "remap", commandWidgetClass, ft_dialog,
	    XtNfromVert, cr_widget,
	    XtNfromHoriz, h_ref,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(remap_widget, remap_flag? dot: no_dot);
    XtAddCallback(remap_widget, XtNcallback, toggle_remap, NULL);
    dialog_register_sensitivity(remap_widget,
	    &ascii_flag, True,
	    NULL, False,
	    NULL, False);

    /* Set up the Units group. */
    units_label = XtVaCreateManagedWidget(
	    "units", labelWidgetClass, ft_dialog,
	    XtNfromVert, append_widget,
	    XtNvertDistance, FAR_VGAP,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
    dialog_register_sensitivity(units_label,
	    &ft_private.receive_flag, False,
	    &host_is_tso, True,
	    NULL, False);

    units_options[0] = XtVaCreateManagedWidget(
	    "spaceDefault", commandWidgetClass, ft_dialog,
	    XtNfromVert, units_label,
	    XtNvertDistance, 3,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(units_options[0],
	    (ft_private.units == DEFAULT_UNITS)? diamond: no_diamond);
    XtAddCallback(units_options[0], XtNcallback,
	    units_callback, (XtPointer)&u_default_units);
    dialog_register_sensitivity(units_options[0],
	    &ft_private.receive_flag, False,
	    &host_is_tso, True,
	    NULL, False);

    units_options[1] = XtVaCreateManagedWidget(
	    "tracks", commandWidgetClass, ft_dialog,
	    XtNfromVert, units_options[0],
	    XtNvertDistance, CLOSE_VGAP,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(units_options[1],
	    (ft_private.units == TRACKS)? diamond: no_diamond);
    XtAddCallback(units_options[1], XtNcallback,
	    units_callback, (XtPointer)&u_tracks);
    dialog_register_sensitivity(units_options[1],
	    &ft_private.receive_flag, False,
	    &host_is_tso, True,
	    NULL, False);

    units_options[2] = XtVaCreateManagedWidget(
	    "cylinders", commandWidgetClass, ft_dialog,
	    XtNfromVert, units_options[1],
	    XtNvertDistance, CLOSE_VGAP,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(units_options[2],
	    (ft_private.units == CYLINDERS)? diamond: no_diamond);
    XtAddCallback(units_options[2], XtNcallback,
	    units_callback, (XtPointer)&u_cylinders);
    dialog_register_sensitivity(units_options[2],
	    &ft_private.receive_flag, False,
	    &host_is_tso, True,
	    NULL, False);

    units_options[3] = XtVaCreateManagedWidget(
	    "avblock", commandWidgetClass, ft_dialog,
	    XtNfromVert, units_options[2],
	    XtNvertDistance, CLOSE_VGAP,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
    dialog_apply_bitmap(units_options[3],
	    (ft_private.units == AVBLOCK)? diamond: no_diamond);
    XtAddCallback(units_options[3], XtNcallback,
	    units_callback, (XtPointer)&u_avblock);
    dialog_register_sensitivity(units_options[3],
	    &ft_private.receive_flag, False,
	    &host_is_tso, True,
	    NULL, False);

    primspace_label = XtVaCreateManagedWidget(
	    "primspace", labelWidgetClass, ft_dialog,
	    XtNfromVert, units_options[3],
	    XtNvertDistance, 3,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
    dialog_register_sensitivity(primspace_label,
	    &ft_private.receive_flag, False,
	    &host_is_tso, True,
	    &units_default, False);
    primspace_widget = XtVaCreateManagedWidget(
	    "value", asciiTextWidgetClass, ft_dialog,
	    XtNfromVert, units_options[3],
	    XtNvertDistance, 3,
	    XtNfromHoriz, primspace_label,
	    XtNhorizDistance, 0,
	    XtNwidth, 100,
	    XtNeditType, XawtextEdit,
	    XtNdisplayCaret, False,
	    NULL);
    dialog_match_dimension(primspace_label, primspace_widget, XtNheight);
    w = XawTextGetSource(primspace_widget);
    if (w == NULL) {
	XtWarning("Cannot find text source in dialog");
    } else {
	XtAddCallback(w, XtNcallback, dialog_text_callback,
		(XtPointer)&t_numeric);
    }
    dialog_register_sensitivity(primspace_widget,
	    &ft_private.receive_flag, False,
	    &host_is_tso, True,
	    &units_default, False);

    secspace_label = XtVaCreateManagedWidget(
	    "secspace", labelWidgetClass, ft_dialog,
	    XtNfromVert, primspace_widget,
	    XtNvertDistance, 3,
	    XtNfromHoriz, h_ref,
	    XtNhorizDistance, COLUMN_GAP,
	    XtNborderWidth, 0,
	    NULL);
    dialog_match_dimension(primspace_label, secspace_label, XtNwidth);
    dialog_register_sensitivity(secspace_label,
	    &ft_private.receive_flag, False,
	    &host_is_tso, True,
	    &units_default, False);
    secspace_widget = XtVaCreateManagedWidget(
	    "value", asciiTextWidgetClass, ft_dialog,
	    XtNfromVert, primspace_widget,
	    XtNvertDistance, 3,
	    XtNfromHoriz, secspace_label,
	    XtNhorizDistance, 0,
	    XtNwidth, 100,
	    XtNeditType, XawtextEdit,
	    XtNdisplayCaret, False,
	    NULL);
    dialog_match_dimension(secspace_label, secspace_widget, XtNheight);
    w = XawTextGetSource(secspace_widget);
    if (w == NULL) {
	XtWarning("Cannot find text source in dialog");
    } else {
	XtAddCallback(w, XtNcallback, dialog_text_callback,
		(XtPointer)&t_numeric);
    }
    dialog_register_sensitivity(secspace_widget,
	    &ft_private.receive_flag, False,
	    &host_is_tso, True,
	    &units_default, False);

    /* Set up the DFT buffer size. */
    buffersize_label = XtVaCreateManagedWidget(
	    "buffersize", labelWidgetClass, ft_dialog,
	    XtNfromVert, blksize_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    buffersize_widget = XtVaCreateManagedWidget(
	    "value", asciiTextWidgetClass, ft_dialog,
	    XtNfromVert, blksize_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNfromHoriz, buffersize_label,
	    XtNhorizDistance, 0,
	    XtNwidth, 100,
	    XtNeditType, XawtextEdit,
	    XtNdisplayCaret, False,
	    NULL);
    dialog_match_dimension(buffersize_label, buffersize_widget, XtNheight);
    w = XawTextGetSource(buffersize_widget);
    if (w == NULL) {
	XtWarning("Cannot find text source in dialog");
    } else {
	XtAddCallback(w, XtNcallback, dialog_text_callback,
		(XtPointer)&t_numeric);
    }
    dialog_register_sensitivity(buffersize_widget,
	    NULL, False,
	    NULL, False,
	    NULL, False);
    set_dft_buffersize();
    s = xs_buffer("%d", dft_buffersize);
    XtVaSetValues(buffersize_widget, XtNstring, s, NULL);
    XtFree(s);

    /* Set up the buttons at the bottom. */
    start_button = XtVaCreateManagedWidget(
	    ObjConfirmButton, commandWidgetClass, ft_dialog,
	    XtNfromVert, buffersize_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    NULL);
    XtAddCallback(start_button, XtNcallback, ft_start_callback, NULL);

    cancel_button = XtVaCreateManagedWidget(
	    ObjCancelButton, commandWidgetClass, ft_dialog,
	    XtNfromVert, buffersize_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNfromHoriz, start_button,
	    XtNhorizDistance, BUTTON_GAP,
	    NULL);
    XtAddCallback(cancel_button, XtNcallback, ft_cancel, 0);
}

/* Callbacks for all the transfer widgets. */

/* Transfer pop-up popping up. */
static void
ft_popup_callback(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    /* Set the focus to the local file widget. */
    PA_dialog_focus_xaction(local_file, NULL, NULL, NULL);

    /* Disallow overwrites. */
    ft_private.allow_overwrite = False;
}

/* Cancel button pushed. */
static void
ft_cancel(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    XtPopdown(ft_shell);
}

/* recfm options. */
static void
recfm_callback(Widget w, XtPointer user_data, XtPointer call_data _is_unused)
{
    ft_private.recfm = *(enum recfm *)user_data;
    recfm_default = (ft_private.recfm == DEFAULT_RECFM);
    dialog_check_sensitivity(&recfm_default);
    dialog_flip_toggles(&recfm_toggles, w);
}

/* Units options. */
static void
units_callback(Widget w, XtPointer user_data, XtPointer call_data _is_unused)
{
    ft_private.units = *(enum units *)user_data;
    units_default = (ft_private.units == DEFAULT_UNITS);
    dialog_check_sensitivity(&units_default);
    dialog_flip_toggles(&units_toggles, w);
}

/* OK button pushed. */
static void
ft_start_callback(Widget w _is_unused, XtPointer call_parms _is_unused,
	XtPointer call_data _is_unused)
{
    XtPopdown(ft_shell);

    if (ft_start()) {
	popup_progress();
    }
}

/* Send/receive options. */
static void
toggle_receive(Widget w _is_unused, XtPointer client_data,
	XtPointer call_data _is_unused)
{
    /* Toggle the flag */
    ft_private.receive_flag = *(Boolean *)client_data;

    /* Change the widget states. */
    dialog_mark_toggle(receive_toggle, ft_private.receive_flag? diamond:
								no_diamond);
    dialog_mark_toggle(send_toggle, ft_private.receive_flag? no_diamond:
							     diamond);
    dialog_check_sensitivity(&ft_private.receive_flag);
}

/* Ascii/binary options. */
static void
toggle_ascii(Widget w _is_unused, XtPointer client_data,
	XtPointer call_data _is_unused)
{
    /* Toggle the flag. */
    ascii_flag = *(Boolean *)client_data;

    /* Change the widget states. */
    dialog_mark_toggle(ascii_toggle, ascii_flag? diamond: no_diamond);
    dialog_mark_toggle(binary_toggle, ascii_flag? no_diamond: diamond);
    cr_flag = ascii_flag;
    remap_flag = ascii_flag;
    dialog_mark_toggle(cr_widget, cr_flag? dot: no_dot);
    dialog_mark_toggle(remap_widget, remap_flag? dot: no_dot);
    dialog_check_sensitivity(&ascii_flag);
}

/* CR option. */
static void
toggle_cr(Widget w, XtPointer client_data _is_unused, XtPointer call_data _is_unused)
{
    /* Toggle the cr flag */
    cr_flag = !cr_flag;

    dialog_mark_toggle(w, cr_flag? dot: no_dot);
}

/* Append option. */
static void
toggle_append(Widget w, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    /* Toggle Append Flag */
    ft_private.append_flag = !ft_private.append_flag;

    dialog_mark_toggle(w, ft_private.append_flag? dot: no_dot);
}

/* Remap option. */
static void
toggle_remap(Widget w, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    /* Toggle Remap Flag */
    remap_flag = !remap_flag;

    dialog_mark_toggle(w, remap_flag? dot: no_dot);
}

/*
 * Set the individual Boolean variables used by the dialog sensitivity
 * functions, and call dialog_check_sensitivity().
 */
static void
set_host_type_booleans(void)
{
    switch (ft_private.host_type) {
    case HT_TSO:
	host_is_tso = True;
	host_is_tso_or_vm = True;
	break;
    case HT_VM:
	host_is_tso = False;
	host_is_tso_or_vm = True;
	break;
    case HT_CICS:
	host_is_tso = False;
	host_is_tso_or_vm = False;
    }

    dialog_check_sensitivity(&host_is_tso);
    dialog_check_sensitivity(&host_is_tso_or_vm);
}

/* TSO/VM/CICS option. */
static void
toggle_host_type(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    host_type_t old_host_type;

    /* Toggle the flag. */
    old_host_type = ft_private.host_type;
    ft_private.host_type = *(host_type_t *)client_data;
    if (ft_private.host_type == old_host_type) {
	return;
    }

    /* Change the widget states. */
    dialog_mark_toggle(vm_toggle,
	    (ft_private.host_type == HT_VM)? diamond: no_diamond);
    dialog_mark_toggle(tso_toggle,
	    (ft_private.host_type == HT_TSO)? diamond: no_diamond);
    dialog_mark_toggle(cics_toggle,
	    (ft_private.host_type == HT_CICS)? diamond: no_diamond);

    if (ft_private.host_type != HT_TSO) {
	/* Reset record format. */
	if ((ft_private.host_type == HT_VM &&
	     ft_private.recfm == RECFM_UNDEFINED) ||
	    (ft_private.host_type == HT_CICS &&
	     ft_private.recfm != DEFAULT_RECFM)) {
	    ft_private.recfm = DEFAULT_RECFM;
	    recfm_default = True;
	    dialog_flip_toggles(&recfm_toggles, recfm_toggles.widgets[0]);
	}
	/* Reset units. */
	if (ft_private.units != DEFAULT_UNITS) {
	    ft_private.units = DEFAULT_UNITS;
	    units_default = True;
	    dialog_flip_toggles(&units_toggles, units_toggles.widgets[0]);
	}
	if (ft_private.host_type == HT_CICS) {
	    /* Reset logical record size and block size. */
	    XtVaSetValues(lrecl_widget, XtNstring, "", NULL);
	    XtVaSetValues(blksize_widget, XtNstring, "", NULL);
	}
	/* Reset primary and secondary space. */
	XtVaSetValues(primspace_widget, XtNstring, "", NULL);
	XtVaSetValues(secspace_widget, XtNstring, "", NULL);
    }

    set_host_type_booleans();
}

/**
 * Begin the transfer.
 *
 * @return True if the transfer has started, False otherwise
 */
static Boolean
ft_start(void)
{
    varbuf_t r;
    String buffersize, lrecl, blksize, primspace, secspace;
    unsigned flen;
    char *s;

    ft_private.is_action = False;

#if defined(X3270_DBCS) /*[*/
    ft_dbcs_state = FT_DBCS_NONE;
#endif /*]*/

    /* Get the DFT buffer size. */
    XtVaGetValues(buffersize_widget, XtNstring, &buffersize, NULL);
    if (*buffersize) {
	dft_buffersize = atoi(buffersize);
    } else {
	dft_buffersize = 0;
    }
    set_dft_buffersize();
    s = xs_buffer("%d", dft_buffersize);
    XtVaSetValues(buffersize_widget, XtNstring, s, NULL);
    XtFree(s);

    /* Get the host file from its widget */
    XtVaGetValues(host_file, XtNstring, &ft_private.host_filename, NULL);
    if (!*ft_private.host_filename) {
	return False;
    }
    /* XXX: probably more validation to do here */

    /* Get the local file from it widget */
    XtVaGetValues(local_file, XtNstring,  &ft_local_filename, NULL);
    if (!*ft_local_filename) {
	return False;
    }

    /* See if the local file can be overwritten. */
    if (ft_private.receive_flag && !ft_private.append_flag &&
	    !ft_private.allow_overwrite) {
	ft_local_file = fopen(ft_local_filename, ascii_flag? "r": "rb");
	if (ft_local_file != NULL) {
	    (void) fclose(ft_local_file);
	    ft_local_file = NULL;
	    popup_overwrite();
	    return False;
	}
    }

    /* Open the local file. */
    ft_local_file = fopen(ft_local_filename, ft_local_fflag());
    if (ft_local_file == NULL) {
	    ft_private.allow_overwrite = False;
	    popup_an_errno(errno, "Local file '%s'", ft_local_filename);
	    return False;
    }

    /* Build the ind$file command */
    vb_init(&r);
    vb_appendf(&r, "IND\\e005BFILE %s %s %s",
	    ft_private.receive_flag? "GET": "PUT",
	    ft_private.host_filename,
	    (ft_private.host_type != HT_TSO)? "(": "");
    if (ascii_flag) {
	vb_appends(&r, "ASCII");
    } else if (ft_private.host_type == HT_CICS) {
	vb_appends(&r, "BINARY");
    }
    if (cr_flag) {
	vb_appends(&r, " CRLF");
    } else if (ft_private.host_type == HT_CICS) {
	vb_appends(&r, " NOCRLF");
    }
    if (ft_private.append_flag && !ft_private.receive_flag) {
	vb_appends(&r, " APPEND");
    }
    if (!ft_private.receive_flag) {
	if (ft_private.host_type == HT_TSO) {
	    if (ft_private.recfm != DEFAULT_RECFM) {
		/* RECFM Entered, process */
		vb_appends(&r, " RECFM(");
		switch (ft_private.recfm) {
		case RECFM_FIXED:
		    vb_appends(&r, "F");
		    break;
		case RECFM_VARIABLE:
		    vb_appends(&r, "V");
		    break;
		case RECFM_UNDEFINED:
		    vb_appends(&r, "U");
		    break;
		default:
		    break;
		};
		vb_appends(&r, ")");
		XtVaGetValues(lrecl_widget, XtNstring, &lrecl, NULL);
		if (strlen(lrecl) > 0) {
		    vb_appendf(&r, " LRECL(%s)", lrecl);
		}
		XtVaGetValues(blksize_widget, XtNstring, &blksize, NULL);
		if (strlen(blksize) > 0) {
		    vb_appendf(&r, " BLKSIZE(%s)", blksize);
		}
	    }
	    if (ft_private.units != DEFAULT_UNITS) {
		/* Space Entered, processs it */
		switch (ft_private.units) {
		case TRACKS:
		    vb_appends(&r, " TRACKS");
		    break;
		case CYLINDERS:
		    vb_appends(&r, " CYLINDERS");
		    break;
		case AVBLOCK:
		    vb_appends(&r, " AVBLOCK");
		    break;
		default:
		    break;
		};
		XtVaGetValues(primspace_widget, XtNstring, &primspace, NULL);
		if (strlen(primspace) > 0) {
		    vb_appendf(&r, " SPACE(%s", primspace);
		    XtVaGetValues(secspace_widget, XtNstring, &secspace, NULL);
		    if (strlen(secspace) > 0) {
			vb_appendf(&r, ",%s", secspace);
		    }
		    vb_appends(&r, ")");
		}
	    }
	} else if (ft_private.host_type == HT_VM) {
	    if (ft_private.recfm != DEFAULT_RECFM) {
		vb_appends(&r, " RECFM ");
		switch (ft_private.recfm) {
		case RECFM_FIXED:
		    vb_appends(&r, "F");
		    break;
		case RECFM_VARIABLE:
		    vb_appends(&r, "V");
		    break;
		default:
		    break;
		};

		XtVaGetValues(lrecl_widget, XtNstring, &lrecl, NULL);
		if (strlen(lrecl) > 0) {
		    vb_appendf(&r, " LRECL %s", lrecl);
		}
	    }
	}
    }
    vb_appends(&r, "\\n");

    /* Erase the line and enter the command. */
    flen = kybd_prime();
    if (!flen || flen < vb_len(&r) - 1) {
	vb_free(&r);
	if (ft_local_file != NULL) {
	    fclose(ft_local_file);
	    ft_local_file = NULL;
	    if (ft_private.receive_flag && !ft_private.append_flag) {
		unlink(ft_local_filename);
	    }
	}
	popup_an_error("%s", get_message("ftUnable"));
	ft_private.allow_overwrite = False;
	return False;
    }
    (void) emulate_input(vb_buf(&r), vb_len(&r), False);
    vb_free(&r);

    /* Get this thing started. */
    ft_state = FT_AWAIT_ACK;
    ft_private.is_cut = False;
    ft_last_cr = False;
#if defined(X3270_DBCS) /*[*/
    ft_last_dbcs = False;
#endif /*]*/

    return True;
}

/* "Transfer in Progress" pop-up. */

/* Pop up the "in progress" pop-up. */
static void
popup_progress(void)
{
    /* Initialize it. */
    if (progress_shell == NULL) {
	progress_popup_init();
    }

    /* Pop it up. */
    popup_popup(progress_shell, XtGrabNone);
}

/* Initialize the "in progress" pop-up. */
static void
progress_popup_init(void)
{
    Widget progress_pop, from_label, to_label, cancel_button;

    /* Create the shell. */
    progress_shell = XtVaCreatePopupShell(
	    "ftProgressPopup", transientShellWidgetClass, toplevel,
	    NULL);
    XtAddCallback(progress_shell, XtNpopupCallback, place_popup,
	    (XtPointer)CenterP);
    XtAddCallback(progress_shell, XtNpopupCallback,
	    progress_popup_callback, NULL);

    /* Create a form structure to contain the other stuff */
    progress_pop = XtVaCreateManagedWidget(
	    ObjDialog, formWidgetClass, progress_shell,
	    NULL);

    /* Create the widgets. */
    from_label = XtVaCreateManagedWidget(
	    "fromLabel", labelWidgetClass, progress_pop,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    from_file = XtVaCreateManagedWidget(
	    "filename", labelWidgetClass, progress_pop,
	    XtNwidth, FILE_WIDTH,
	    XtNvertDistance, FAR_VGAP,
	    XtNfromHoriz, from_label,
	    XtNhorizDistance, 0,
	    NULL);
    dialog_match_dimension(from_label, from_file, XtNheight);

    to_label = XtVaCreateManagedWidget(
	    "toLabel", labelWidgetClass, progress_pop,
	    XtNfromVert, from_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
    to_file = XtVaCreateManagedWidget(
	    "filename", labelWidgetClass, progress_pop,
	    XtNwidth, FILE_WIDTH,
	    XtNfromVert, from_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNfromHoriz, to_label,
	    XtNhorizDistance, 0,
	    NULL);
    dialog_match_dimension(to_label, to_file, XtNheight);

    dialog_match_dimension(from_label, to_label, XtNwidth);

    waiting = XtVaCreateManagedWidget(
	    "waiting", labelWidgetClass, progress_pop,
	    XtNfromVert, to_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    XtNmappedWhenManaged, False,
	    NULL);

    ft_status = XtVaCreateManagedWidget(
	    "status", labelWidgetClass, progress_pop,
	    XtNfromVert, to_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    XtNresizable, True,
	    XtNmappedWhenManaged, False,
	    NULL);
    XtVaGetValues(ft_status, XtNlabel, &status_string, NULL);
    status_string = XtNewString(status_string);

    aborting = XtVaCreateManagedWidget(
	    "aborting", labelWidgetClass, progress_pop,
	    XtNfromVert, to_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    XtNmappedWhenManaged, False,
	    NULL);

    cancel_button = XtVaCreateManagedWidget(
	    ObjCancelButton, commandWidgetClass, progress_pop,
	    XtNfromVert, ft_status,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    NULL);
    XtAddCallback(cancel_button, XtNcallback, progress_cancel_callback, NULL);
}

/* Callbacks for the "in progress" pop-up. */

/* In-progress pop-up popped up. */
static void
progress_popup_callback(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    XtVaSetValues(from_file, XtNlabel,
	    ft_private.receive_flag? ft_private.host_filename:
				     ft_local_filename,
	    NULL);
    XtVaSetValues(to_file, XtNlabel,
	    ft_private.receive_flag? ft_local_filename:
				     ft_private.host_filename,
	    NULL);

    switch (ft_state) {
    case FT_AWAIT_ACK:
	XtUnmapWidget(ft_status);
	XtUnmapWidget(aborting);
	XtMapWidget(waiting);
	break;
    case FT_RUNNING:
	XtUnmapWidget(waiting);
	XtUnmapWidget(aborting);
	XtMapWidget(ft_status);
	break;
    case FT_ABORT_WAIT:
    case FT_ABORT_SENT:
	XtUnmapWidget(waiting);
	XtUnmapWidget(ft_status);
	XtMapWidget(aborting);
	break;
    default:
	break;
    }
}

/* In-progress "cancel" button. */
static void
progress_cancel_callback(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    if (ft_state == FT_RUNNING) {
	ft_state = FT_ABORT_WAIT;
	XtUnmapWidget(waiting);
	XtUnmapWidget(ft_status);
	XtMapWidget(aborting);
    } else {
	/* Impatient user or hung host -- just clean up. */
	ft_complete(get_message("ftUserCancel"));
    }
}

/* "Overwrite existing?" pop-up. */

/* Pop up the "overwrite" pop-up. */
static void
popup_overwrite(void)
{
    /* Initialize it. */
    if (overwrite_shell == NULL) {
	overwrite_popup_init();
    }

    /* Pop it up. */
    popup_popup(overwrite_shell, XtGrabExclusive);
}

/* Initialize the "overwrite" pop-up. */
static void
overwrite_popup_init(void)
{
    Widget overwrite_pop, overwrite_name, okay_button, cancel_button;
    String overwrite_string, label, lf;
    Dimension d;

    /* Create the shell. */
    overwrite_shell = XtVaCreatePopupShell(
	    "ftOverwritePopup", transientShellWidgetClass, toplevel,
	    NULL);
    XtAddCallback(overwrite_shell, XtNpopupCallback, place_popup,
	    (XtPointer)CenterP);
    XtAddCallback(overwrite_shell, XtNpopdownCallback, overwrite_popdown,
	    NULL);

    /* Create a form structure to contain the other stuff */
    overwrite_pop = XtVaCreateManagedWidget(
	    ObjDialog, formWidgetClass, overwrite_shell,
	    NULL);

    /* Create the widgets. */
    overwrite_name = XtVaCreateManagedWidget(
	    "overwriteName", labelWidgetClass, overwrite_pop,
	    XtNvertDistance, MARGIN,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    XtNresizable, True,
	    NULL);
    XtVaGetValues(overwrite_name, XtNlabel, &overwrite_string, NULL);
    XtVaGetValues(local_file, XtNstring, &lf, NULL);
    label = xs_buffer(overwrite_string, lf);
    XtVaSetValues(overwrite_name, XtNlabel, label, NULL);
    XtFree(label);
    XtVaGetValues(overwrite_name, XtNwidth, &d, NULL);
    if ((Dimension)(d + 20) < 400) {
	d = 400;
    } else {
	d += 20;
    }
    XtVaSetValues(overwrite_name, XtNwidth, d, NULL);
    XtVaGetValues(overwrite_name, XtNheight, &d, NULL);
    XtVaSetValues(overwrite_name, XtNheight, d + 20, NULL);

    okay_button = XtVaCreateManagedWidget(
	    ObjConfirmButton, commandWidgetClass, overwrite_pop,
	    XtNfromVert, overwrite_name,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    NULL);
    XtAddCallback(okay_button, XtNcallback, overwrite_okay_callback, NULL);

    cancel_button = XtVaCreateManagedWidget(
	    ObjCancelButton, commandWidgetClass, overwrite_pop,
	    XtNfromVert, overwrite_name,
	    XtNvertDistance, FAR_VGAP,
	    XtNfromHoriz, okay_button,
	    XtNhorizDistance, BUTTON_GAP,
	    NULL);
    XtAddCallback(cancel_button, XtNcallback, overwrite_cancel_callback, NULL);
}

/* Overwrite "okay" button. */
static void
overwrite_okay_callback(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    XtPopdown(overwrite_shell);

    ft_private.allow_overwrite = True;
    if (ft_start()) {
	XtPopdown(ft_shell);
	popup_progress();
    }
}

/* Overwrite "cancel" button. */
static void
overwrite_cancel_callback(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    XtPopdown(overwrite_shell);
}

/* Overwrite pop-up popped down. */
static void
overwrite_popdown(Widget w _is_unused, XtPointer client_data _is_unused,
	XtPointer call_data _is_unused)
{
    XtDestroyWidget(overwrite_shell);
    overwrite_shell = NULL;
}

/* Entry points called from the common FT logic. */

/* Pop down the transfer-in-progress pop-up. */
void
ft_gui_progress_popdown(void)
{
    if (!ft_private.is_action) {
	XtPopdown(progress_shell);
    }
}

/* Massage a file transfer error message so it will fit in the pop-up. */
#define MAX_MSGLEN 50
void
ft_gui_errmsg_prepare(char *msg)
{
    if (strlen(msg) > MAX_MSGLEN && strchr(msg, '\n') == NULL) {
	char *s = msg + MAX_MSGLEN;
	while (s > msg && *s != ' ') {
	    s--;
	}
	if (s > msg) {
	    *s = '\n';      /* yikes! */
	}
    }
}

/* Clear out the progress display. */
void
ft_gui_clear_progress(void)
{
}

/* Pop up a successful completion message. */
void
ft_gui_complete_popup(const char *msg)
{
    popup_an_info("%s", msg);
}

/* Update the bytes-transferred count on the progress pop-up. */
void
ft_gui_update_length(unsigned long length)
{
    if (!ft_private.is_action) {
	char *s = xs_buffer(status_string, length);

	XtVaSetValues(ft_status, XtNlabel, s, NULL);
	XtFree(s);
    }
}

/* Replace the 'waiting' pop-up with the 'in-progress' pop-up. */
void
ft_gui_running(unsigned long length)
{
    if (!ft_private.is_action) {
	XtUnmapWidget(waiting);
	ft_gui_update_length(length);
	XtMapWidget(ft_status);
    }
}

/* Process a protocol-generated abort. */
void
ft_gui_aborting(void)
{
    if (!ft_private.is_action) {
	XtUnmapWidget(waiting);
	XtUnmapWidget(ft_status);
	XtMapWidget(aborting);
    }
}

/* Check for interactive mode. */
Boolean
ft_gui_interact(String **params _is_unused, Cardinal *num_params _is_unused)
{
    return False;
}

/* Display an "Awaiting start of transfer" message. */
void
ft_gui_awaiting(void)
{
}