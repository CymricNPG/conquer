/*conquer : Copyright (c) 1988 by Ed Barlow.
 *  I spent a long time writing this code & I hope that you respect this.
 *  I give permission to alter the code, but not to copy or redistribute
 *  it without my explicit permission.  If you alter the code,
 *  please document changes and send me a copy, so all can have it.
 *  This code, to the best of my knowledge works well,  but it is my first
 *  'C' program and should be treated as such.  I disclaim any
 *  responsibility for the codes actions (use at your own risk).  I guess
 *  I am saying "Happy gaming", and am trying not to get sued in the process.
 *                                                Ed
 */

#include	<ctype.h>
#include	<errno.h>
#include	"header.h"
#include	"data.h"
#include	"patchlevel.h"

#include	<signal.h>
#include	<pwd.h>

extern	int armornvy;

char	fison[20];
char	*getpass();
struct	s_sector **sct;
struct	s_nation ntn[NTOTAL];	/* player nation stats */
struct	s_world	world;		
char	**occ;	/*is sector occupied by an army?*/
short	**movecost;
long	startgold=0;
long	mercgot=0;

short	xoffset=0,yoffset=0;	/*offset of upper left hand corner*/
/* current cursor postion (relative to 00 in upper corner) */
/*	position is 2*x,y*/
short	xcurs=0,ycurs=0;
short	redraw=TRUE;	/* if TRUE: redraw map		*/
int	done=FALSE;	/* if TRUE: you are done	*/
short	hilmode=HI_OWN;	/* hilight mode */
short	dismode=DI_DESI;/* display mode			*/
short	selector=0;	/* selector (y vbl) for which army/navy... is "picked"*/
short	pager=0;	/* pager for selector 0,1,2,3*/
short	country=0;	/* nation id of owner*/
struct	s_nation	*curntn;
int	owneruid;

FILE *fexe, *fopen();

/************************************************************************/
/*	MAIN() - main loop for conquer					*/
/************************************************************************/
void
main(argc,argv)
int	argc;
char	**argv;
{
	int geteuid(), getuid(), setuid();
	register int i;
	char name[NAMELTH+1],filename[80];
	void srand(),init_hasseen();
	int getopt();
	char passwd[PASSLTH+1];
	long time();
	extern char *optarg, conqmail[];
#ifdef SYSMAIL
	extern char sysmail[];
#endif SYSMAIL
	int sflag=FALSE;

	char defaultdir[256],tmppass[PASSLTH+1];
	struct passwd *getpwnam();
	owneruid=getuid();
	strcpy(defaultdir, DEFAULTDIR);
	srand((unsigned) time((long *) 0));
	strcpy(name,"");

	/* process the command line arguments */
	while((i=getopt(argc,argv,"hn:d:s"))!=EOF) switch(i){
	/* process the command line arguments */
	case 'h': /* execute help program*/
		if (chdir(defaultdir)) {
			printf("unable to change dir to %s\n",defaultdir);
			exit(FAIL);
		}
		initscr();
		savetty();
		noecho();
		crmode();			/* cbreak mode */
		signal(SIGINT,SIG_IGN);		/* disable keyboard signals */
		signal(SIGQUIT,SIG_IGN);
		help();
		endwin();
		putchar('\n');
		exit(SUCCESS);
	case 'd':
		if(optarg[0]!='/') {
			sprintf(defaultdir, "%s/%s", DEFAULTDIR, optarg);
		} else {
			strcpy(defaultdir, optarg);
		}
		break;
	case 'n':
		strcpy(name, optarg);
		break;
	case 's': /*print the score*/
		sflag++;
		break;
	case '?': /*  print out command line arguments */
		printf("Command line format: %s [-hs -d DIR -nNAT]\n",argv[0]);
		printf("\t-n NAT   play as nation NAT\n");
		printf("\t-h       print help text\n");
		printf("\t-d DIR   to use play different game\n");
		printf("\t-s       print scores\n");
		exit(SUCCESS);
	};

	/* now that we have parsed the args, we can go to the
	 * dir where the files are kept and do some work.
	 */
	if (chdir(defaultdir)) {
		printf("unable to change dir to %s\n",defaultdir);
		exit(FAIL);
	}

	readdata();				/* read data*/
	verifydata( __FILE__, __LINE__ );	/* verify data */

	if(sflag){
		printscore();
		exit(SUCCESS);
	}

	/*
	*  Set the real uid to the effective.  This will avoid a
	*  number of problems involving file protection if the
	*  executable is setuid.
	*/
	if (getuid() != geteuid()) { /* we are running suid */
		(void) umask(077);	/* nobody else can read files */
		(void) setuid (geteuid ()) ;
	}

	/* at this stage must be a normal interactive game */

	printf("conquer %s.%d: Copyright (c) 1988 by Edward M Barlow\n",VERSION,PATCHLEVEL);

	/* check for update in progress */
	sprintf(filename,"%sup",isonfile);
	if(check_lock(filename,FALSE)==TRUE) {
		printf("Conquer is updating\n");
		printf("Please try again later.\n");
		exit(FAIL);
	}

	/* identify player and country represented */
	/* get nation name from command line or by asking user.
	*     if you fail give name of administrator of game
	*/
	if (strlen(name) == 0) {
		printf("what nation would you like to be: ");
		gets(name);
	}
#ifdef OGOD
	if(strcmp(name,"god")==0 || strcmp(name,"unowned")==0) {
		if ( owneruid != (getpwnam(LOGIN))->pw_uid ){
			printf("sorry -- you can not login as god\n");
			printf("you need to be logged in as %s\n",LOGIN);
			exit(FAIL);
		}
		strcpy(name,"unowned");
		hilmode = HI_NONE;
	}
#else
	if(strcmp(name,"god")==0) strcpy(name,"unowned");
#endif OGOD
	country=(-1);
	for(i=0;i<NTOTAL;i++)
		if(strcmp(name,ntn[i].name)==0) country=i;

	if(country==(-1)) {
		printf("name not found\n");
		printf("\nfor rules type <conquer -h>");
		printf("\nfor more information please contact %s\n",OWNER);
		return;
	} else if(country==0) {
		sprintf(filename,"%sadd",isonfile);
		if(check_lock(filename,FALSE)==TRUE) {
			printf("A new player is being added.\n");
			printf("Continue anyway? [y or n]");
			while(((i=getchar())!='y')&&(i!='n')) ;
			if(i!='y') exit(FAIL);
		}
	}
	curntn = &ntn[country];

	/*get encrypted password*/
	strncpy(tmppass,getpass("\nwhat is your nation's password:"),PASSLTH);
	strncpy(passwd,crypt(tmppass,SALT),PASSLTH);
	if((strncmp(passwd,curntn->passwd,PASSLTH)!=0)
	&&(strncmp(passwd,ntn[0].passwd,PASSLTH)!=0)) {
		strncpy(tmppass,getpass("\nerror: reenter your nation's password:"),PASSLTH);
		strncpy(passwd,crypt(tmppass,SALT),PASSLTH);
		if((strncmp(passwd,curntn->passwd,PASSLTH)!=0)
		&&(strncmp(passwd,ntn[0].passwd,PASSLTH)!=0)) {
			printf("\nsorry:");
			printf("\nfor rules type <conquer -h>");
			printf("\nfor more information on the system please contact %s\n",OWNER);
			exit(FAIL);
		}
	}

	initscr();		/* SET UP THE SCREEN */
	/* check terminal size */
	if (COLS<80 || LINES<24) {
		fprintf(stderr,"%s: terminal should be at least 80x24\n",argv[0]);
		fprintf(stderr,"please try again with a different setup\n");
		beep();
		getch();
		bye(FALSE);
	}

	copyscreen();		/* copyright screen */
				/* note the getch() later - everything between
					now and then is non-interactive */
	init_hasseen();		/* now we know how big the screen is, 
					we can init that array!	*/

	strcpy(fison,"START");	/* just in case you abort early */
	crmode();		/* cbreak mode */

	/* check if user is super-user nation[0] */
	/*	else setup cursor to capitol*/
	if((country==0)||(ismonst(ntn[country].active))) {
		xcurs=LINES/2;
		xoffset=0;
		ycurs=COLS/4;
		yoffset=0;
		redraw=TRUE;
		/* create gods lock file but do not limit access */
		(void) aretheyon();
	} else {
		if(curntn->active==INACTIVE) {
			mvprintw(LINES-2,0,"Sorry, for some reason, your country no longer exists.");
			mvprintw(LINES-1,0,"If there is a problem, please contact %s.",OWNER);
			beep();
			refresh();
			getch();
			bye(TRUE);
		}
		if(aretheyon()==TRUE) {
			mvprintw(LINES-2,0,"Sorry, country is already logged in.");
			mvprintw(LINES-1,0,"Please try again later.    ");
			beep();
			refresh();
			getch();
			bye(FALSE);
		}
		execute(FALSE);
#ifdef TRADE
		checktrade();
#endif TRADE
		if(curntn->capx>15) {
			xcurs=15;
			xoffset= ((int)curntn->capx-15);
		} else {
			xcurs= curntn->capx;
			xoffset= 0;
		}
		if(curntn->capy>10) {
			ycurs=10;
			yoffset= ((int)curntn->capy-10);
		} else {
			yoffset= 0;
			ycurs= curntn->capy;
		}
	}
	updmove(curntn->race,country);

	/* open output for future printing*/
	sprintf(filename,"%s%d",exefile,country);
	if ((fexe=fopen(filename,"a"))==NULL) {
		beep();
		mvprintw(LINES-2,0,"error opening %s",filename);
		refresh();
		getch();
		bye(TRUE);
	}


	signal(SIGINT,SIG_IGN);		/* disable keyboard signals */
	signal(SIGQUIT,SIG_IGN);
	signal(SIGHUP,hangup);		/* must catch hangups */
	signal(SIGTERM,hangup);		/* likewise for cheats!! */

	noecho();
	prep(country,FALSE);		/* initialize prep array */
	whatcansee();			/* what can they see */

	/* initialize mail files */
	(void) sprintf(conqmail,"%s%d",msgfile,country);
#ifdef SYSMAIL
	if (getenv("MAIL")==0) {
		(void) sprintf(sysmail,"%s/%s",SPOOLDIR,getenv("USER"));
	} else {
		(void) strcpy(sysmail,getenv("MAIL"));
	}
#endif SYSMAIL

	getch();		/* get response from copyscreen */

	while(done==FALSE) {			/*main while routine*/
		coffmap(); 	/* check if cursor is out of bounds*/
		check_mail();	/* check for new mail */
		parse();	/* get commands, make moves and input command*/
	}

	if(country==0) writedata();
	else {
		fprintf(fexe,"L_NGOLD\t%d \t%d \t%ld \t0 \t0 \t%s\n",
			XNAGOLD ,country,curntn->tgold,"null");
		fprintf(fexe,"L_NMETAL\t%d \t%d \t%ld \t0 \t0 \t%s\n",
			XNAMETAL ,country,curntn->metals,"null");
		fprintf(fexe,"L_NJWLS\t%d \t%d \t%ld \t0 \t0 \t%s\n",
			XNARGOLD ,country,curntn->jewels,"null");
	}
	bye(TRUE);	 		/* done so quit */
}

/************************************************************************/
/* MAKEBOTTOM() - make the bottom of the screen				*/
/************************************************************************/
void
makebottom()
{
	standend();
	move(LINES-4,0);
	clrtoeol();
	mvprintw(LINES-3,0,"Conquer: %s.%d Turn %d",VERSION,PATCHLEVEL,TURN);
	clrtoeol();
	mvaddstr(LINES-1,0,"  type ? for help");
	clrtoeol();
	mvaddstr(LINES-2,0,"  type Q to save & quit");
	clrtoeol();

	if(country==0) {
		mvaddstr(LINES-3,COLS-20,"nation...GOD");
	} else {
		mvprintw(LINES-3,COLS-20,"nation...%s",curntn->name);
		mvprintw(LINES-2,COLS-20,"treasury.%ld",curntn->tgold);
	}
	mvprintw(LINES-1,COLS-20,"%s of Year %d",PSEASON(TURN),YEAR(TURN));

	/* mail status */
#ifdef SYSMAIL
	/* display mail information */
	if (sys_mail_status==NEW_MAIL) {
		mvaddstr(LINES-3,COLS/2-6,"You have System Mail");
	}
	if (conq_mail_status==NEW_MAIL) {
		mvaddstr(LINES-2,COLS/2-6,"You have Conquer Mail");
	}
#else
	/* display mail information */
	if (conq_mail_status==NEW_MAIL) {
		mvaddstr(LINES-3,COLS/2-6,"You have Conquer Mail");
	}
#endif SYSMAIL
}

/************************************************************************/
/*	PARSE() - do a command						*/
/************************************************************************/
void
parse()
{
	char	name[20];
	char	passwd[PASSLTH+1];
	int	ocountry;

	switch(getch()) {
	case EXT_CMD:	/* extended command */
		ext_cmd( -1 );
		curntn->tgold -= MOVECOST;
		break;
	case '':	/*redraw the screen*/
		whatcansee();	/* what can they see */
		redraw=TRUE;
		break;
	case 'a':	/*army report*/
		redraw=TRUE;
		armyrpt(0);
		curntn->tgold -= MOVECOST;
		break;
	case '1':
	case 'b':	/*move south west*/
		pager=0;
		selector=0;
		xcurs--;
		ycurs++;
		break;
	case 'B':	/*budget*/
		redraw=TRUE;
		budget();
		curntn->tgold -= MOVECOST;
		break;
	case 'c':	/*change nation stats*/
		redraw=TRUE;
		change();
		curntn->tgold -= MOVECOST;
		break;
	case 'C':	/*construct*/
		construct();
		makebottom();
		curntn->tgold -= MOVECOST;
		break;
	case 'd':	/*change display*/
		newdisplay();
		curntn->tgold -= MOVECOST;
		break;
	case 'D':	/*draft*/
		draft();
		curntn->tgold -= MOVECOST;
		makebottom();
		break;
	case 'f': /*report on ships and load/unload*/
		redraw=TRUE;
		curntn->tgold -= MOVECOST;
		fleetrpt();
		break;
	case 'F':	/*go to next army*/
		navygoto();
		break;
	case 'g':	/*group report*/
		redraw=TRUE;
		curntn->tgold -= MOVECOST;
		armyrpt(1);
		break;
	case 'G':	/*go to next army*/
		armygoto();
		break;
	case 'H':	/*scroll west*/
		pager=0;
		selector=0;
		xcurs-=((COLS-22)/4);
		break;
	case '4':
	case 'h':	/*move west*/
		pager=0;
		selector=0;
		xcurs--;
		break;
	case 'I':	/*campaign information*/
		camp_info();
		redraw=TRUE;
		break;
	case 'J':	/*scroll down*/
		pager=0;
		selector=0;
		ycurs+=((SCREEN_Y_SIZE)/2);
		break;
	case '2':
	case 'j':	/*move down*/
		pager=0;
		selector=0;
		ycurs++;
		break;
	case '8':
	case 'k':	/*move up*/
		pager=0;
		selector=0;
		ycurs--;
		break;
	case 'K':	/*scroll up*/
		pager=0;
		selector=0;
		ycurs-=((SCREEN_Y_SIZE)/2);
		break;
	case '6':
	case 'l':	/*move east*/
		pager=0;
		selector=0;
		xcurs++;
		break;
	case 'L':	/*scroll east*/
		pager=0;
		selector=0;
		xcurs+=((COLS-22)/4);
		break;
	case 'm':	/*move selected item to new x,y */
		mymove();
		curntn->tgold -= MOVECOST;
		makebottom();
		prep(country,FALSE);
		pager=0;
		selector=0;
		break;
	case 'M':	/*magic*/
		redraw=TRUE;
		curntn->tgold -= MOVECOST;
		domagic();
		break;
	case '3':
	case 'n':	/*move south-east*/
		pager=0;
		selector=0;
		ycurs++;
		xcurs++;
		break;
	case 'N':	/*read newspaper */
		redraw=TRUE;
		curntn->tgold -= MOVECOST;
		newspaper();
		break;
	case 'p':	/*pick*/
		selector+=2;
		if(selector>=SCRARM*2) {
			selector=0;
			pager+=1;
		}
		/*current selected unit is selector/2+SCRARM*pager*/
		if((selector/2)+(pager*SCRARM)>=units_in_sector(XREAL,YREAL,country)) {
			pager=0;
			selector=0;
		}
		break;
	case 'P':	/*production*/
		redraw=TRUE;
		curntn->tgold -= MOVECOST;
		produce();
		break;
	case 'Q':	/*quit*/
	case 'q':	/*quit*/
		done=TRUE;
		break;
	case 'r':	/*redesignate*/
		redesignate();
		curntn->tgold -= MOVECOST;
		makemap();
		makebottom();
		break;
		/*list*/
	case 'R':	/*Read Messages*/
		redraw=TRUE;
		curntn->tgold -= MOVECOST;
		rmessage();
		refresh();
		break;
	case 's':	/*score*/
		redraw=TRUE;
		curntn->tgold -= MOVECOST;
		showscore();
		break;
	case 'S':	/*diplomacy screens*/
		diploscrn();
		curntn->tgold -= MOVECOST;
		redraw=TRUE;
		break;
	case 't':	/*fleet loading*/
		loadfleet();
		curntn->tgold -= MOVECOST;
		makeside(FALSE);
		makebottom();
		break;
#ifdef TRADE
	case 'T':	/*go to commerce section*/
		trade();
		curntn->tgold -= MOVECOST;
		redraw=TRUE;
		break;
#endif TRADE
	case '9':
	case 'u':	/*move north-east*/
		pager=0;
		selector=0;
		ycurs--;
		xcurs++;
		break;
    	case 'U':	/* scroll north-east */
		pager=0;
		selector=0;
		xcurs+=((COLS-22)/4);
		ycurs-=((SCREEN_Y_SIZE)/2);
		break;
    	case 'v':	/* version credits */
		credits();
		redraw=TRUE;
		break;
	case 'w':	/* spell casting */
		wizardry();
		curntn->tgold -= MOVECOST;
		break;
	case 'W':	/*message*/
		redraw=TRUE;
		curntn->tgold -= MOVECOST;
		wmessage();
		break;
	case '7':
	case 'y':	/*move north-west*/
		pager=0;
		selector=0;
		ycurs--;
		xcurs--;
		break;
	case 'Y':	/* scroll north-west */
		pager=0;
		selector=0;
		xcurs-=((COLS-22)/4);
		ycurs-=((SCREEN_Y_SIZE)/2);
		break;
	case 'Z':	/*move civilians up to 2 spaces*/
		moveciv();
		curntn->tgold -= MOVECOST;
		break;
	case 'z':	/*login as new user */
#ifdef OGOD
		if (owneruid != (getpwnam(LOGIN))->pw_uid) break;
#endif
		clear();
		redraw=TRUE;
		if(country != 0) {
		fprintf(fexe,"L_NGOLD\t%d \t%d \t%ld \t0 \t0 \t%s\n",
			XNAGOLD ,country,curntn->tgold,"null");
		fprintf(fexe,"L_NMETAL\t%d \t%d \t%ld \t0 \t0 \t%s\n",
			XNAMETAL ,country,curntn->metals,"null");
		fprintf(fexe,"L_NJWLS\t%d \t%d \t%ld \t0 \t0 \t%s\n",
			XNARGOLD ,country,curntn->jewels,"null");
		} else
		mvaddstr(0,0,"SUPER-USER: YOUR CHANGES WILL NOT BE SAVED IF YOU DO THIS!!!");
		standout();
		mvaddstr(2,0,"change login to: ");
		standend();
		refresh();

		ocountry=country;
 		country=get_country();

		/* check validity of country choice */
		if( country==(-1) || country>=NTOTAL
		|| ( !isactive(ntn[country].active) && country!=0 )) {
			country=ocountry;
			errormsg("invalid country");
			break;
		} 
		if(country==ocountry){
			errormsg("same country");
			break;
		}

		/*get password*/
		mvaddstr(2,0,"what is your nations password:");
		refresh();
		getstr(passwd);
		strcpy(name,crypt(passwd,SALT));
		if((strncmp(name,curntn->passwd,PASSLTH)!=0)
		&&(strncmp(name,ntn[0].passwd,PASSLTH)!=0)){
			errormsg("sorry:  password invalid");
			country=ocountry;
			break;
		}
		if(aretheyon()==TRUE) {
			errormsg("sorry:  country is already logged in.");
			refresh();
			country=ocountry;
			break;
		}

		if(strcmp(fison,"START")!=0) unlink(fison);

		fclose(fexe);
		/* open output for future printing*/
	 	sprintf(name,"%s%d",exefile,country);
	 	if ((fexe=fopen(name,"a"))==NULL) {
			beep();
			printf("error opening %s\n",name);
			unlink(fison);
			exit(FAIL);
	 	}
		curntn = &ntn[country];

		printf("\n");
		readdata();
		execute(FALSE);

		(void) sprintf(conqmail,"%s%d",msgfile,country);
		updmove(curntn->race,country);
		/*go to that nations capitol*/
		if((country==0)||(!isntn(ntn[country].active))) {
			xcurs=15; xoffset=15;
			ycurs=15; yoffset=15;
		} else {
			if(curntn->capx>15) {
				xcurs=15;
				xoffset= ((int)curntn->capx-15);
			} else {
				xcurs= curntn->capx;
				xoffset= 0;
			}
			if(curntn->capy>10) {
				ycurs=10;
				yoffset= ((int)curntn->capy-10);
			} else {
				yoffset= 0;
				ycurs= curntn->capy;
			}
		}
		whatcansee();
		redraw=TRUE;
		break;
	case '?':	/*display help screen*/
		redraw=TRUE;
		help();
		break;
	default:
		beep();
	}
}

/************************************************************************/
/*	MAKESIDE() -	make the right hand side display		*/
/************************************************************************/
void
makeside(alwayssee)
int	alwayssee;	/* see even if cant really see sector */
{
	int	i;
	int	armbonus;
	int	found=0,nvyfnd=0;
	long	enemy;
	int	y;
	short	armynum;
	short	nvynum;
	int	count;
	int	nfound=0,nfound2=0;
	register struct s_sector	*sptr = &sct[XREAL][YREAL];

	if( !alwayssee )
	if( !canbeseen((int) XREAL,(int) YREAL) ) {
		for(i=0;i<LINES-3;i++){
			move(i,COLS-21);
			clrtoeol();
		}
		return;
	}

	for(count=0;count<LINES-13;count++){	/*clear top right hand side */
		move(count,COLS-21);
		clrtoeol();
	}

	/*check for your armies*/
	count=units_in_sector(XREAL,YREAL,country);
	if(pager*SCRARM>count) pager=0;

	/*first army found is #0*/
	/*show armies / navies in range pager*SCRARM to pager*SCRARM + SCRARM*/
	/*so if pager=0 show 0 to 5 (SCRARM), pager=2 show 10 to 15*/
	/*current selected unit is selector/2+4*pager*/

	if(count>(SCRARM+(pager*SCRARM))) mvaddstr(LINES-14,COLS-20,"MORE...");

	nfound=0;
	for(armynum=0;armynum<MAXARM;armynum++){
		if((P_ASOLD>0)&&(P_AXLOC==XREAL)&&(P_AYLOC==YREAL)) {
			if((nfound>=pager*SCRARM)&&(nfound<SCRARM+(pager*SCRARM))) {
				/*print that army to nfound%SCRARM*/
				/* patch by rob mayoff */
				if(selector==(nfound%SCRARM)*2) {
					mvaddch((nfound%SCRARM)*2,COLS-21,'*');
					standout();
				} else	mvaddch((nfound%SCRARM)*2,COLS-21,'>');

				if(P_ATYPE<MINLEADER)
				mvprintw((nfound%SCRARM)*2,COLS-20,"army %d: %ld %s",armynum,P_ASOLD,*(shunittype+(P_ATYPE%UTYPE)));
				else 
				mvprintw((nfound%SCRARM)*2,COLS-20,"%s %d: str=%d",*(unittype+(P_ATYPE%UTYPE)),armynum,P_ASOLD);
				clrtoeol();

				if(P_ASTAT >= NUMSTATUS )
				mvprintw((nfound%SCRARM)*2+1,COLS-20," member group %d",P_ASTAT-NUMSTATUS);
				else
				mvprintw((nfound%SCRARM)*2+1,COLS-20," mv:%d st:%s",P_AMOVE,*(soldname+P_ASTAT));
				standend();
			}
			nfound++;
		}
		if((occ[XREAL][YREAL]!=0)
		&&(occ[XREAL][YREAL]!=country)
		&&((sptr->owner==country)||((P_ASOLD>0)&&(P_AXLOC<=XREAL+1)
		&&(P_AXLOC>=XREAL-1)&&(P_AYLOC<=YREAL+1)&&(P_AYLOC>=YREAL-1))))
			found=1;
		if((occ[XREAL][YREAL]!=0)&&(country==0)) found=1;
	}

	if(nfound<SCRARM+(pager*SCRARM)) for(nvynum=0;nvynum<MAXNAVY;nvynum++){
		if(((P_NWSHP!=0)||(P_NMSHP!=0)||(P_NGSHP!=0))
		&&(P_NXLOC==XREAL)&&(P_NYLOC==YREAL)) {
			if((nfound>=pager*SCRARM)&&(nfound<SCRARM+(pager*SCRARM))) {
				/*print a navy*/
				if(selector==(nfound%SCRARM)*2) {
					if((P_NARMY!=MAXARM)||(P_NPEOP!=0))
					mvaddch((nfound%SCRARM)*2,COLS-21,'+');
					else
					mvaddch((nfound%SCRARM)*2,COLS-21,'*');
					standout();
				} else	mvaddch((nfound%SCRARM)*2,COLS-21,'>');
	
				mvprintw((nfound%SCRARM)*2,COLS-20,"nvy %d: mv:%hd cw:%hd",nvynum,P_NMOVE,P_NCREW);
				mvprintw((nfound%SCRARM)*2+1,COLS-20,"war:%2hd mer:%2hd gal:%2hd",
					P_NWAR(N_LIGHT)+P_NWAR(N_MEDIUM)+P_NWAR(N_HEAVY),
					P_NMER(N_LIGHT)+P_NMER(N_MEDIUM)+P_NMER(N_HEAVY),
					P_NGAL(N_LIGHT)+P_NGAL(N_MEDIUM)+P_NGAL(N_HEAVY));
				standend();
			}
			nfound++;
		}
		if((occ[XREAL][YREAL]!=0)&&(occ[XREAL][YREAL]!=country)
		&&(P_NWSHP!=0||P_NMSHP!=0||P_NGSHP!=0)&&(P_NXLOC<=XREAL+1)
		&&(P_NXLOC>=XREAL-1)&&(P_NYLOC<=YREAL+1)&&(P_NYLOC>=YREAL-1))
			nvyfnd=1;
		if((occ[XREAL][YREAL]!=0)&&(country==0)) nvyfnd=1;
	}

	count=0;
	nfound2=nfound;
	if((found==1)||(nvyfnd==1)) for(i=0;i<NTOTAL;i++) {
		if( !magic(i,HIDDEN) || country == 0 ){
			enemy=0;
			for(armynum=0;armynum<MAXARM;armynum++){
				if((i!=country)
				&&(ntn[i].arm[armynum].xloc==XREAL)
				&&(ntn[i].arm[armynum].yloc==YREAL)
				&&(ntn[i].arm[armynum].sold>0)){
				if(nfound2>SCRARM) nfound2=SCRARM;
				if( ntn[i].arm[armynum].unittyp>=MINMONSTER ){
					mvprintw(nfound2*2+count,COLS-20,"%s: str=%d",*(unittype+(ntn[i].arm[armynum].unittyp%UTYPE)),ntn[i].arm[armynum].sold);
					count++;
				} else enemy += ntn[i].arm[armynum].sold;
				}
			}
			if(enemy>0) {
				if((magic(country,NINJA)==TRUE) || country == 0 )
					mvprintw(nfound2*2+count,COLS-20,"%s: %d men  ",ntn[i].name,enemy);
				else if(magic(i,THE_VOID)==TRUE)
				mvprintw(nfound2*2+count,COLS-20,"%s: ?? men  ",ntn[i].name);
				else mvprintw(nfound2*2+count,COLS-20,"%s: %ld men  ",ntn[i].name,(enemy*(rand()%60+70)/100));
				count++;
			}
			enemy=0;
			for(nvynum=0;nvynum<MAXNAVY;nvynum++){
				if((i!=country)
				&&(ntn[i].nvy[nvynum].xloc==XREAL)
				&&(ntn[i].nvy[nvynum].yloc==YREAL)
				&&(ntn[i].nvy[nvynum].warships
				+ntn[i].nvy[nvynum].merchant
				+(int)ntn[i].nvy[nvynum].galleys!=0))
					enemy += fltships(i,nvynum);
				}
			if(enemy>0) {
				if((magic(country,NINJA)==TRUE) || country == 0 )
					mvprintw(nfound2*2+count,COLS-20,"%s: %d ships",ntn[i].name,enemy);
				else if(magic(i,THE_VOID)==TRUE)
				mvprintw(nfound2*2+count,COLS-20,"%s: ?? ships",ntn[i].name);
				else mvprintw(nfound2*2+count,COLS-20,"%s: %ld ships",ntn[i].name,(enemy*(rand()%60+70)/100));
				count++;
			}
		}
	}

	standend();
	mvprintw(LINES-13,COLS-20,"x is %d",XREAL);
	clrtoeol();
	mvprintw(LINES-13,COLS-11,"y is %d",YREAL);
	clrtoeol();

	if((country!=0)&&(sptr->altitude==WATER)){
		for(y=LINES-12;y<=LINES-4;y++) {	move(y,COLS-20); clrtoeol();}
		mvaddstr(LINES-10,COLS-9,"WATER");
	} else {
	if((country!=0)&&(country!=sptr->owner)
	&&(magic(sptr->owner,THE_VOID)==TRUE)){
		for(y=LINES-11;y<=LINES-4;y++) { 
			move(y,COLS-20);
			clrtoeol();
		}
	} else {

		for(y=LINES-11;y<=LINES-10;y++) {
			move(y,COLS-20);
			clrtoeol();
		}

		if( sptr->designation!=DNODESIG ) standout();
		for(i=0;*(des+i)!='0';i++)
			if(sptr->designation== *(des+i)){
			mvprintw(LINES-11,COLS-20,"%s",*(desname+i));
			clrtoeol();
			break;
		}
		standend();

		if((sptr->owner==country)||(country==0)||(magic(country,NINJA)==TRUE))
		mvprintw(LINES-9,COLS-20,"people: %6d",sptr->people);
		else
		mvprintw(LINES-9,COLS-20,"people: %6d",sptr->people*(rand()%60+70)/100);
		clrtoeol();
		if((sptr->owner==country)
		||(sptr->owner==0)
		||(country == 0)
		||(!isntn(ntn[sptr->owner].active))){
			/* exotic trade goods */
			if( sptr->tradegood != TG_none && tg_ok(country,sptr) ) {
				standout();
				mvprintw(LINES-7,COLS-20,"item: %s",tg_name[sptr->tradegood]);
				clrtoeol();
				if( *(tg_stype+sptr->tradegood) == 'x' )
					mvaddstr(LINES-7,COLS-4,"ANY");
				else
					mvprintw(LINES-7,COLS-4,"(%c)",*(tg_stype+sptr->tradegood));
				standend();
			} else {
				mvaddstr(LINES-7,COLS-20,"item: none");
				clrtoeol();
			}

			if( sptr->jewels != 0 && tg_ok(country,sptr)) {
				standout();
				mvprintw(LINES-6,COLS-20,"gold: %2d",sptr->jewels);
				standend();
			} else mvaddstr(LINES-6,COLS-20,"gold:  0");
			if( sptr->metal != 0 && tg_ok(country,sptr)) {
				standout();
				mvprintw(LINES-6,COLS-10,"metal: %2d",sptr->metal);
				standend();
			} else mvaddstr(LINES-6,COLS-10,"metal:  0");

			armbonus = fort_val(sptr);
			if(armbonus>0)
			mvprintw(LINES-5,COLS-20,"fortress: +%d%%",armbonus);
			else move(LINES-5,COLS-20);
			clrtoeol();
		}
		else {
			for(y=LINES-7;y<=LINES-5;y++) {
				move(y,COLS-20);
				clrtoeol();
			}
		}
	}

	standout();
	if((sptr->owner==0)||(ntn[sptr->owner].active==NPC_BARBARIAN))
		mvaddstr(LINES-12,COLS-20,"unowned");
	else mvprintw(LINES-12,COLS-20,"owner: %s",ntn[sptr->owner].name);
	standend();
	clrtoeol();

	for(i=0;*(veg+i)!='0';i++)
		if(sptr->vegetation==*(veg+i))
		mvprintw(LINES-11,COLS-9,"%s",*(vegname+i));

	if(((i=tofood(sptr,country)) != 0)
	&&((magic(sptr->owner,THE_VOID)!=TRUE)
	||(sptr->owner==country))){
		if(i>6) standout();
#ifndef HPUX
		if(i<10)	mvprintw(LINES-11,COLS-1,"%d",i);
		else		mvprintw(LINES-11,COLS-2,"%d",i);
#else
		if(i<10)	mvprintw(LINES-11,COLS-2,"%d",i);
		else		mvprintw(LINES-11,COLS-3,"%d",i);
#endif HPUX
		standend();
	}

	if(sptr->owner!=0) for(i=1;i<=8;i++)
		if(ntn[sptr->owner].race==*(races+i)[0]){
		mvprintw(LINES-10,COLS-20,"%s",*(races+i));
		clrtoeol();
		}

	for(i=0;(*(ele+i) != '0');i++)
		if( sptr->altitude == *(ele+i) ){
			mvprintw(LINES-10,COLS-9,"%s",*(elename+i));
			break;
		}
	}

	if(movecost[XREAL][YREAL]<0)
	mvaddstr(LINES-8,COLS-20,"YOU CAN'T ENTER HERE");
	else
	mvprintw(LINES-8,COLS-20,"move cost:  %2d      ",movecost[XREAL][YREAL]);
}

/************************************************************************/
/* 	ARETHEYON() - returns TRUE if 'country' is logged on, else FALSE */
/************************************************************************/
int
aretheyon()
{
	/* return file descriptor for lock file */
	sprintf(fison,"%s%d",isonfile,country);
	return(check_lock(fison,TRUE));
}

/************************************************************************/
/*	COPYSCREEN() -	print copyright notice to screen		*/
/* THIS SUBROUTINE MAY NOT BE ALTERED, AND THE MESSAGE CONTAINED HEREIN	*/
/* MUST BE SHOWN TO EACH AND EVERY PLAYER, EVERY TIME THEY LOG IN	*/
/************************************************************************/
void
copyscreen()
{
#ifdef TIMELOG
	FILE *timefp, *fopen();
	char string[80];
#endif /* TIMELOG */

	clear();
	standout();
	mvprintw(8,COLS/2-12,"Conquer %s.%d",VERSION,PATCHLEVEL);
	standend();
	mvaddstr(10,COLS/2-21, "Copyright (c) 1988 by Edward M Barlow");
	mvaddstr(11,COLS/2-22,"Written Edward M Barlow and Adam Bryant");
	mvaddstr(12,COLS/2-12,"All Rights Reserved");
	mvaddstr(LINES-8,COLS/2-21,"This version is for personal use only");
	mvaddstr(LINES-6,COLS/2-32,"It is expressly forbidden port this software to any form of");
	mvaddstr(LINES-5,COLS/2-32,"Personal Computer or to redistribute this software without");
	mvaddstr(LINES-4,COLS/2-26,"the permission of Edward Barlow or Adam Bryant");
#ifdef TIMELOG
	if ((timefp=fopen(timefile,"r"))!=NULL) {
		fgets(string, 50, timefp);
		mvprintw(LINES-1, 0, "Last Update: %s", string);
		fclose(timefp);
	}
#endif /* TIMELOG */
	mvprintw(LINES-1, COLS-20, "PRESS ANY KEY");
	refresh();
}

/************************************************************************/
/*	BYE()	-	exit gracefully from curses			*/
/************************************************************************/
void
bye(dounlink)
int	dounlink;	/* TRUE if want to do unlink */
{
	if( dounlink ) if(strcmp(fison,"START")!=0) unlink(fison);
	clear();
	refresh();
	nocrmode();
	endwin();
	if (fexe!=NULL) fclose(fexe);
	printf("quit & save\n");
	exit(SUCCESS);
}

/************************************************************************/
/*	CREDITS() -	print credits notice to screen			*/
/************************************************************************/
void
credits()
{
	clear();
	mvprintw(4,0,"Conquer %s.%d",VERSION,PATCHLEVEL);
	mvaddstr(5,0,"Copyright (c) 1988 by Edward M Barlow");
	mvaddstr(6,0,"written Edward M Barlow and Adam Bryant");
	mvaddstr(12,0,"I would like to thank the following for comments,");
	mvaddstr(13,0,"   patches, and playtesting:");
	mvaddstr(15,0,"Derick Hirasawa    Brian Rauchfuss      Joe E. Powell");
	mvaddstr(16,0,"Andrew Collins     Joe Nolet");
	mvaddstr(17,0,"Kenneth Moyle      Brian Bresnahan");
	mvaddstr(18,0,"Paul Davison       Robert Deroy");
	mvaddstr(20,0,"Also thanks to the many playtesters at Boston University");
	mvaddstr(21,0,"and at the Communications Hex");
	errormsg("");
}

/************************************************************************/
/*	CAMP_INFO() -	display information about current data file	*/
/************************************************************************/
void
camp_info()
{
	int mercs=0,solds=0,armynum,nvynum;
	int numarm=0,numnvy=0,numlead=0;

	clear();
	standout();
	mvaddstr(2,COLS/2-16," CONQUER CAMPAIGN INFORMATION ");
	mvaddstr(5,0,"World Information");
	mvaddstr(5,COLS-40,"Player Information");
	standend();

	/* quick statistics */
	for(armynum=0;armynum<MAXARM;armynum++) {
		if (P_ASOLD!=0) {
			numarm++;
			if (P_ATYPE<MINLEADER) {
				solds+=P_ASOLD;
				if (P_ATYPE==A_MERCENARY) mercs+=P_ASOLD;
			} else if (P_ATYPE<MINMONSTER) {
				numlead++;
			}
		}
	}
	for(nvynum=0;nvynum<MAXNAVY;nvynum++) {
		if (P_NWSHP!=0||P_NGSHP!=0||P_NMSHP!=0) numnvy++;
	}

	/* global information */
	mvprintw(7,0,"World Map Size............. %dx%d", MAPX, MAPY);
	mvprintw(8,0,"Currently Active Nations... %d", WORLDNTN);
	mvprintw(9,0,"Maximum Active Nations..... %d", NTOTAL-1);
	mvprintw(10,0,"Land displacement to meet.. %d", MEETNTN);
	mvprintw(11,0,"Maximum Number of Armies... %d", MAXARM);
	mvprintw(12,0,"Maximum Number of Navies... %d", MAXNAVY);
	mvprintw(13,0,"Chance of Scout Capture.... %d%%", PFINDSCOUT);

	/* user information */
	mvprintw(7,COLS-40,"Number of Leaders........... %d",numlead);
	mvprintw(8,COLS-40,"Men Needed To Take Land..... %d",TAKESECTOR);
	mvprintw(9,COLS-40,"Mercenaries in Nation....... %d",mercs);
	mvprintw(10,COLS-40,"Total Soldiers in Nation.... %d",solds);
	mvprintw(11,COLS-40,"Current Number of Armies.... %d",numarm);
	mvprintw(12,COLS-40,"Current Number of Navies.... %d",numnvy);

	standout();
	mvaddstr(LINES-2,COLS/2-13," HIT ANY KEY TO CONTINUE");
	standend();
	refresh();

	getch();
}
