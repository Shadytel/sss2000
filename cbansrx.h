/**********************************************************************
 *       Multi-line Asynchronous Answering Phone Demo Program        *
 *********************************************************************
 *                                                                   *
 *    CCCC   BBBBB     AA    N    N   SSSS   RRRRR   x   x      H  H *
 *   C    C  B    B   A  A   NN   N  S       R    R   x x       H  H *
 *   C       BBBBB   A    A  N N  N   SSSS   R    R    x        H  H *
 *   C       B    B  AAAAAA  N  N N       S  RRRRR     x   ...  HHHH *
 *   C    C  B    B  A    A  N   NN  S    S  R   R    x x  ...  H  H *
 *    CCCC   BBBBB   A    A  N    N   SSSS   R    R  x   x ...  H  H *
 *                                                                   *
 *********************************************************************
 *  Copyright (c) 1992-96 by Dialogic Corp.  All Rights Reserved     *
 *********************************************************************
 *  DESCRIPTION:  This file contains the function prototypes for     *
 *   procedures used by the dialogic call back handler thread.       *
 *********************************************************************/



/**
 ** Definitions
 **/

#define MAXCHANS    47   /* Max Number of Channels */
#define MAXDTMF         4       /* Number of Digits Expected        */
#define MAXRING         1       /* Number of Rings Before Picking Up*/
#define MAXMSG          31      /* Max Length of Message Filename   */
#define FALSE           0
#define TRUE            1
#define SIGEVTCHK       0x1010  /* Check for type of signaling event*/
#define SIGBITCHK       0x1111  /* Check for signalling bit change  */

#define MAX_STR_LEN 255
#define MAX_ARGS 20

/*
 * Vox Files to Open
 */
#define INTRO_VOX       "INTRO.VOX"
#define INVALID_VOX     "INVALID.VOX"
#define GOODBYE_VOX     "GOODBYE.VOX"
#define ERROR_VOX       "ERROR.VOX"
/*
 * Definition of states
 */

#define ST_BLOCKED  0   /* For ISDN circuits; channel not available */
#define ST_WTRING	1	/* Waiting for an Incoming Call    */
#define ST_OFFHOOK	2	/* Going Off Hook to Accept Call   */
#define ST_INTRO	3	/* Play the intro.vox File         */
#define ST_GETDIGIT	4	/* Get DTMF Digits (Access Code)   */
#define ST_PLAY		5	/* Play the Caller's Message File  */
#define ST_RECORD	6	/* Recording Message from Caller   */
#define ST_INVALID	7	/* Play invalid.vox (Invalid Code) */
#define ST_GOODBYE	8	/* Play goodbye.vox                */
#define ST_ONHOOK	9	/* Go On Hook                      */
#define ST_ERROR	10	/* An Error has been Encountered   */
#define ST_DYNPLAY_DTMF	11	/* Playing DISA tone		   */
#define ST_DIGPROC	12	/* Playtone getdigits stuff 	   */
#define ST_DYNDIG	13	/* Dynamic sound player digit context */
#define ST_DYNPLAY	14	/* Dynamic sound player play context */
#define ST_ENIGMAREC	15	/* Long record function for Enigma */
#define ST_ENIGMAREC2	16	/* Editing and stuff for Enigmarec */
#define ST_EVANS1	17
#define ST_EVANS2	18
#define ST_EVANS3	19
#define ST_OUTDIALSB	20 // if ( dxinfo[ channum ].state > ST_OUTDIALSB ) tpt[ 0 ].tp_length = DM_P;
#define ST_CATREC	21
#define ST_CATREC2	22
#define ST_CATREC3	23
#define ST_CATCREATE	24
#define ST_GETCAT	25
#define ST_GETCAT2	26
#define ST_GETCAT3	27
#define ST_CATNOEXIST	28
#define ST_ENTERPASS	29
#define ST_PASSCREATE	30
#define ST_PASSCREATE2	31
#define ST_PASSCREATE3	32
#define ST_CATMENU	33
#define ST_CATMENU2	34
#define ST_CATRESUME	35
#define ST_OUTDIAL	36
#define ST_OUTDIAL2	37
#define ST_OUTDIAL3	38
#define ST_ANAC		39
#define ST_PASSREADBACK	40
#define ST_MSGREAD	41
#define ST_PLAYNWN	42
#define ST_WINK		43
#define ST_WINKDIG	44
#define ST_MAKEMARK	45
#define ST_RESUMEMARK	46
#define ST_RESUMEMARK2	47
#define ST_RESUMEMARK3	48
#define ST_MSGREC	49
#define ST_MSGREC2	50
#define ST_MSGREC3	51
#define ST_EMREC1	52
#define ST_EMREC2	53
#define ST_EMREC3	54
#define ST_EMPLAY1	55
#define ST_EMPLAY2	56
#define ST_SASTROLL	57 // Swapped for function that increments state values to eliminate resource waste
#define ST_VMAIL1	58
#define ST_VMAIL2	59
#define ST_VMAIL3	60
#define ST_VMAIL4	61
#define ST_VMAIL5	62
#define ST_VMAILPASS	63
#define ST_CALLPTEST4L	64
#define ST_ROUTEDREC	65
#define ST_EMPLAY3	66
#define ST_CRUDEDIAL	67
#define ST_CRUDEDIAL2	68
#define ST_ROUTED	69
#define ST_ROUTED2	70
#define ST_TXDATA	71
#define ST_ISDNTEST	72
#define ST_ISDNERR	73
#define ST_ISDNERR2	74
#define ST_ISDNTEST_ENDCAUSE 75
#define ST_ISDNTEST_ENDCAUSE2 76
#define ST_ISDNTEST_CPNDREAD 77
#define ST_ISDNTEST_CPNDREAD2 78
#define ST_ISDNTEST_CPNREAD	 79
#define ST_ISDNTEST_CPNREAD2 80
#define ST_ISDNTEST_CPNREAD3 81
#define ST_ISDNTEST_CPTYPE	 82
#define ST_ISDNTEST_CPTYPE2	 83
#define ST_ISDNTEST_NUTYPE	 84
#define ST_ISDNTEST_NUTYPE2	 85
#define ST_ISDNTEST_TEMPMENU 86
#define ST_FAKECONF1	     87
#define ST_FAKECONF2	     88
#define ST_FAKECONF3	     89
#define ST_FAKECONF_ERR	     90
#define ST_PLAYLOOP			 91
#define ST_VMAILPASS1		 92
#define ST_VMAILSETUP1		 93
#define ST_VMAILSETUP1E		 94
#define ST_VMAILSETUP1C		 95
#define ST_VMAILSETUP2		 96
#define ST_VMAILSETGREC		 97
#define ST_VMAILGRECEDIT1    98
#define ST_VMAILGRECEDIT1E	 99
#define ST_VMAILGRECEDIT2	100
#define ST_VMAILGRECEDIT3   101
#define ST_VMAILCHECK1		102
#define ST_TONETEST			103
#define ST_TONETEST2		104
#define ST_VMAILCHECK4		105
#define ST_VMAILMENU		106
#define ST_VMAILMENU2		107
#define ST_VMREADBACK		108
// #define ST_VMAILRNEW2		109
#define ST_VMAILHEADER4		110
#define ST_VMAILHEADER3		111
#define ST_VMAILHEADER2		112
#define ST_VMAILHEADER		113
#define ST_VMAILRNEW		114
#define ST_VMAILRNEW2		115
#define ST_VMAILRNEW3		116
#define ST_VMAILRNEW4		117
#define ST_VMAILRSAVED		118
// These need to be developed further within the application
#define ST_VMAILCOMP		119
#define ST_VMAILSETM		120
#define ST_VMAILSETMP		121
#define ST_VMAILTYP			122
#define ST_VMAILTYP2		123
#define ST_VMAILNPASS1		124
#define ST_VMAILNPASS1C		125
#define ST_VMAILNPASS1E		126
#define ST_VMAILSETGREC2	127
#define ST_COLLCALL			128
#define ST_COLLCALL2		129
#define ST_COLLCALL3		130
#define ST_CONFCONF			131
#define ST_CALLPTEST		132
#define ST_CALLPTESTE		133
#define ST_CALLPTEST2		134
#define ST_CALLPTEST2E		135
#define ST_CALLPTEST3		136
#define ST_CALLPTEST3E		137
#define ST_CALLPTEST4		138
#define ST_CALLPTEST5		139
#define ST_PLAYMULTI		140
#define ST_PLAYMULTI1		141
#define ST_MODEMDETECT      142
#define ST_VMBDETECT	    143
#define ST_ISDNROUTE		144
#define ST_ISDNROUTE1		145
#define ST_CONFWAIT			146
#define ST_EVANSDM3			147
#define ST_CONNTEST			148
#define ST_FXSTEST1         149
#define ST_REORDER          150
#define ST_BUSY             151
#define ST_RINGPHONE1       152
#define ST_RINGPHONE2       153
#define ST_INCOMING         154
#define ST_PERMSIG          155
#define ST_DYNPLAYE         156 // dx_fileclose pitches a _huge_ fit and segfaults if you try to close a closed file, but won't indicate the descriptor is closed in any way. This is for a workaround; if you hit an error or other situation, it won't try and close the file descriptor.
#define ST_FXOOUT           157
#define ST_FXOOUT_S         158 // For the FXS side of the outdial
#define ST_FXODISA          159
#define ST_FXORING          160
#define ST_ROUTEDISDN       161
#define ST_ROUTEDISDN2      162
#define ST_ISDNOUT          163
#define ST_ISDNOUT2         164
#define ST_WARBLE           165
#define ST_ERRORANNC        166
#define ST_PERMANNC         167
#define ST_PERMANNC2        168
#define ST_ACBANNC          169
#define ST_RETURNDIGS       170

/*
 * Cast type for event handler functions to avoid warning from SVR4 compiler.
 */
typedef long int (*EVTHDLRTYP)();

/**
 **   Data structure which defines the states for each channel
 **/
typedef struct dx_info {
   int	    chdev;			/* Channel Device Descriptor    */
   int	    tsdev;			/* Timeslot Device Descriptor   */
   int	    state;			/* State of Channel             */
   int	    msg_fd;			/* File Desc for Message Files  */
   DV_DIGIT digbuf;			/* Buffer for DTMF Digits       */
   DX_IOTT  iott[ 1 ];			/* I/O Transfer Table		*/
   char	    msg_name[ MAXMSG+1 ];	/* Message Filename             */
   char	    ac_code[ MAXDTMF+1 ];	/* Access Code for this Channel */
} DX_INFO;

typedef struct isdn {
    char dnis[128]; /* Called number. These first two fields should have the maximum defined with limits from documetation */
    char cpn[128]; // This is wasteful, I know. But this is what the Diva does for MAX_ADDR_LEN, and it seems reasonable enough to prevent overflow on any spec.
    char displayie[83];
    unsigned char prescreen; // Screen bit. For CPN.
    unsigned char bcap[13]; // Maximum length is 12 octets
    unsigned char chanid[7]; // Maximum length is 6 octets? Depends on network, but it's generally three.
    unsigned char progind[5]; // Maximum length is 4 octets
    unsigned char calledtype;
    unsigned char callingtype;
    unsigned char forwardedtype;
    unsigned char forwardedscn;
    unsigned char forwardedrsn;
    unsigned char forwarddata; 
    char status;
    char forwardednum[22]; // Ensure there isn't an off-by-one here
} Q931SIG;

int ringphone( int channum, unsigned char linedev );
int playtone_hdlr();
int dial_hdlr();



/*
 * GLOBALS FOAR JESUS
 */

extern char isdnstatus[ MAXCHANS + 1 ];
extern char ownies[ MAXCHANS + 1];
extern char q931debug;
extern Q931SIG isdninfo[ MAXCHANS + 1];
extern DX_INFO		dxinfox[ MAXCHANS+1 ];
extern short connchan[MAXCHANS + 1];
extern short startchan; // This is for the start of ISDN channels
extern short isdnmax;
extern short isdnbdnum; // Starting board to use for ISDN channels
extern char fxo[MAXCHANS + 1];
extern char	tmpbuff[ 256 ];


/********************************************************************
 *        NAME: void sys_quit()
 * DESCRIPTION: Handler called when one of the following signals is
 *		This function stops I/O activity on all channels and
 *		closes all the channels.
 *       INPUT: None
 *      OUTPUT: None.
 *     RETURNS: None.
 *    CAUTIONS: None.
 ********************************************************************/
void sys_quit();


/*********************************************************************
 *        NAME: int get_channum( chtsdev )
 * DESCRIPTION: Get the index into the dxinfo[] for the channel or timeslot
 *		device descriptor, chtsdev.
 *       INPUT: int chtsdev;	- Channel/Timeslot Device Descriptor
 *      OUTPUT: None.
 *     RETURNS: Returns the index into dxinfo[]
 *    CAUTIONS: None.
 ********************************************************************/
int get_channum( int chtsdev );

/*********************************************************************
 *        NAME: int play( channum, filedesc )
 * DESCRIPTION: Set up IOTT and TPT's and Initiate the Play-Back
 *       INPUT: int channum;	- Index into dxinfo structure
 *		int filedesc;	- File Descriptor of VOX file to Play-Back
 *      OUTPUT: Starts the play-back
 *     RETURNS: -1 = Error
 *		 0 = Success
 *    CAUTIONS: None
 *********************************************************************/
int play( int channum, int filedesc, int format, unsigned long offset );


/*********************************************************************
 *        NAME: int record( channum, filedesc )
 * DESCRIPTION: Set up IOTT and TPT's and Initiate the record
 *       INPUT: int channum;	- Index into dxinfo structure
 *		int filedesc;	- File Descriptor of VOX file to Record to
 *      OUTPUT: Starts the Recording
 *     RETURNS: -1 = Error
 *		 0 = Success
 *    CAUTIONS: None
 ********************************************************************/
int record( int channum, int filedesc );

/********************************************************************
 *        NAME: int get_digits( channum, digbufp )
 * DESCRIPTION: Set up TPT's and Initiate get-digits function
 *       INPUT: int channum;		- Index into dxinfo structure
 *		DV_DIGIT *digbufp;	- Pointer to Digit Buffer
 *      OUTPUT: Starts to get the DTMF Digits
 *     RETURNS: -1 = Error
 *		 0 = Success
 *    CAUTIONS: None
 ********************************************************************/
int get_digits( int channum, DV_DIGIT * digbufp, unsigned char numdigs );

/********************************************************************
 *        NAME: int set_hkstate( channum, state )
 * DESCRIPTION: Set the channel to the appropriate hook status
 *       INPUT: int channum;		- Index into dxinfo structure
 *		int state;		- State to set channel to
 *      OUTPUT: None.
 *     RETURNS: -1 = Error
 *		 0 = Success
 *    CAUTIONS: None.
 ********************************************************************/
int set_hkstate( int channum, int state );

/*********************************************************************
 *        NAME: int cst_hdlr()
 * DESCRIPTION: TDX_CST event handler
 *       INPUT: None.
 *      OUTPUT: None.
 *     RETURNS: 0 if event to be eaten by SRL otherwise 1
 *    CAUTIONS: None.
 *********************************************************************/
int cst_hdlr();

/**********************************************************************
 *        NAME: int play_hdlr()
 * DESCRIPTION: TDX_PLAY event handler
 *       INPUT: None.
 *      OUTPUT: None.
 *     RETURNS: 0 if event to be eaten by SRL otherwise 1
 *    CAUTIONS: None.
 *********************************************************************/
int play_hdlr();

/**********************************************************************
 *        NAME: int record_hdlr()
 * DESCRIPTION: TDX_RECORD event handler
 *       INPUT: None.
 *      OUTPUT: None.
 *     RETURNS: 0 if event to be eaten by SRL otherwise 1
 *    CAUTIONS: None.
 *********************************************************************/
int record_hdlr();


/*********************************************************************
 *        NAME: int getdig_hdlr()
 * DESCRIPTION: TDX_GETDIG event handler
 *       INPUT: None.
 *      OUTPUT: None.
 *     RETURNS: 0 if event to be eaten by SRL otherwise 1
 *    CAUTIONS: None.
 *********************************************************************/
int getdig_hdlr();

/*********************************************************************
 *        NAME: int sethook_hdlr()
 * DESCRIPTION: TDX_SETHOOK event handler
 *       INPUT: None.
 *      OUTPUT: None.
 *     RETURNS: 0 if event to be eaten by SRL otherwise 1
 *    CAUTIONS: None.
 *********************************************************************/
int sethook_hdlr();

/**********************************************************************
 *        NAME: int error_hdlr()
 * DESCRIPTION: TDX_ERROR event handler
 *       INPUT: None.
 *      OUTPUT: None.
 *     RETURNS: 0 if event to be eaten by SRL otherwise 1
 *    CAUTIONS: None.
 *********************************************************************/
int error_hdlr();


/*********************************************************************
 *        NAME: int fallback_hdlr()
 * DESCRIPTION: Fall-Back event handler
 *       INPUT: None.
 *      OUTPUT: None.
 *     RETURNS: 0 if event to be eaten by SRL otherwise 1
 *    CAUTIONS: None.
 ********************************************************************/
int fallback_hdlr();

/*********************************************************************
 *        NAME: int sig_hdlr()
 * DESCRIPTION: Signal handler to catch DTEV_SIG events generated by the dti
 *              timeslots.
 *       INPUT: None.
 *      OUTPUT: None.
 *     RETURNS: 0 if event to be eaten by SRL otherwise 1
 *    CAUTIONS: None.
 ********************************************************************/
int sig_hdlr();

/*********************************************************************
 *        NAME: int nostop_hdlr()
 * DESCRIPTION: Signal handler to catch TDX_NOSTOP events generated by
 *              asynchronous dx_stopch() calls that do nothing.
 *       INPUT: None.
 *      OUTPUT: None.
 *     RETURNS: 0 if event to be eaten by SRL otherwise 1
 *    CAUTIONS: None.
 ********************************************************************/
int nostop_hdlr();


/********************************************************************
 *        NAME: int dtierr_hdlr()
 * DESCRIPTION: Signal handler to catch DTEV_ERREVT events generated by the
 *              dti timeslots.
 *       INPUT: None.
 *      OUTPUT: None.
 *     RETURNS: 0 if event to be eaten by SRL otherwise 1
 *    CAUTIONS: None.
 *********************************************************************/
int dtierr_hdlr( );

/*********************************************************************
 *        NAME: void chkargs()
 * DESCRIPTION: Check options that were sleected
 *		channels to use.
 *       INPUT: None
 *      OUTPUT: None.
 *     RETURNS: None.
 *    CAUTIONS: None.
 **********************************************************************/
void chkargs();

/***************************************************************************
 *        NAME: void sysinit()
 * DESCRIPTION: Start D/4x System, Enable CST events, put line ON-Hook
 *		and open VOX files.
 *       INPUT: None.
 *      OUTPUT: None.
 *     RETURNS: None.
 *    CAUTIONS: None.
 ***************************************************************************/
void sysinit();

char countup( char *numstring );
int send_bell202( int channum, int filedesc );
int digread( int channum, char * digstring );
int dopen (const char * filep, int flags );
void disp_status (int chnum, char *stringp);
void disp_err( int chan_num, int chfd, char *error );
int isdn_inroute( int channum );
char makecall(short channum, char *destination, char *callingnum);
char set_cpn(short channum, char *number, char plan_type, char screen_pres);
