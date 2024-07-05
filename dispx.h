/*********************************************************************
 *       Multi-line Asynchronous Answering Phone Demo Program        *
 *********************************************************************
 *                                                                   *
 *   DCCCC   IIIIII   SSSSS  PPPPP   X   X      H    H               *    
 *   D    D    II    S       P    P  X   X      H    H               *
 *   D    D    II    S       P    P   XXX       HHHHHH               *
 *   D    D    II    SSSSS   PPPPP    XXX   ... H    H               *
 *   D    D    II         S  P       X   X  ... H    H               *
 *   DDDDD   IIIIII  SSSSS   P       X   X  ... H    H               *
 *                                                                   *
 *********************************************************************
 *  Copyright (c) 1997 by Dialogic Corp.  All Rights Reserved        *
 *********************************************************************
 *  DESCRIPTION:  This file contains the function prototypes for     *
 *   procedures used to display text to the client area of main      *
 *   window.                                                         *
 *********************************************************************/


/**********************************************************************
 *        NAME: disp_status(chnum, stringp)
 *      INPUTS: chno - channel number (1 - 30)
 *              stringp - pointer to string to display 
 * DESCRIPTION: display the current activity on the channel in 
 *				window 2 (the string pointed to by stringp) using 
 *              chno as a Y offset
 **********************************************************************/
void disp_status(int chnum, char *strinp);



/********************************************************************** 
 *       NAME:  disp_msg(stringp)
 *      INPUTS: stringp - pointer to string to display.
 * DESCRIPTION: display the string passed, in the primary message 
 *              area of win2
 **********************************************************************/
void disp_msg(char *stringp);


/***********************************************************************
 *        NAME: disp_err( chfd )
 * DESCRIPTION: This routine prints error information.
 *      INPUTS: thr_num - thread number
 *		chfd - device descriptor
 *     OUTPUTS: The error code and error message are displayed
 *    CAUTIONS: none. 
 **********************************************************************/
void disp_err(int channum, int chfd, char *state);
