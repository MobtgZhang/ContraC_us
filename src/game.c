#include "game.h"
#include "sound.h"
#include "graphics.h"
#include "globals.h"
#include "player.h"
#include "enemies.h"

void check_title_keys(void);
void dec_title_delay(void);
void set_title(void);

void main_sub0(void);
void main_sub1(void);
void main_sub2(void);
void main_sub3(void);
void main_sub4(void);
void main_sub5(void);
void main_sub6(void);

SUB main_subs[]={
	main_sub0, main_sub1, main_sub2, main_sub3,
	main_sub4, main_sub5, main_sub6
};

void game_sub0(void);
void game_sub1(void);
void game_sub2(void);
void game_sub3(void);
void game_sub4(void);
void game_sub5(void);
void game_sub6(void);
void game_sub7(void);
void game_sub8(void);
void game_sub9(void);
void game_suba(void);


SUB game_subs[]={
	game_sub0, game_sub1, game_sub2, game_sub3, game_sub4,
	game_sub5, game_sub6, game_sub7, game_sub8, game_sub9,
	game_suba
};

void update_game_frame(void){
	framecount++;
	update_pals();
	proc_msg700();
	vcontol = blank_screens && --blank_screens ? 0 : vcontol_v;
	Scrollx = hscroll;
	Scrolly = vscroll;
	read_keys();
	if (main_proc == 1)
		check_title_keys();
	main_subs[main_proc]();
	disp_objs();
}

void check_title_keys(void){
	dec_title_delay();
	if (player.triggers & 0x10){
		long_delay=0x240;
		if (hscroll){
			set_title();
			return;
		}
		simple_f=0;
		main_proc=3;
	}
}
void dec_title_delay(void){
	if (!(framecount&1))
		if (title_delay)
			title_delay--;
}

void set_title(void){
	hscroll=0;
	title_delay=0xa4;
	gfx_setop(0x20,0);
	write_msg(4);
	write_msg(0x19);
	PLAYSOUND0(TITLE_SND);
}

void main_sub0(void){
	gfx_setop(0,1);//清名字表
	pre_title_init();
	konamicode_idx=0;
	hscroll=0x100;
	long_delay=0x280;
	main_proc++;
}

void main_sub1(void){
	if(hscroll==0){
		disp_forms[0]=0xab;
		chr_xs[0]=0xb3;
		chr_ys[0]=0x77;
		disp_attr[0]=0;
		disp_forms[1]=0xaa;
		chr_xs[1]=0x1b;
		chr_ys[1]=0xb1;
		disp_attr[1]=0;
	}else{
		hscroll++;
		if (hscroll==0x200)
			set_title();
	}
}

void main_sub2(void){
}

void main_sub3(void){
	int msg=2;
	int era_f;

	if (!simple_f){
		long_delay=0x40;
		simple_f++;
	}else{
		dec_title_delay();
		if (long_delay)
			long_delay--;
		if (!long_delay && !title_delay)
			main_proc++;
		else{
			era_f =(framecount&8)<<4;
			msg |= era_f;
			write_msg(msg);
		}
	}
}

void main_sub4(void){
	pre_game_init(true);
	main_proc++;
}

void main_sub5(void){
	game_subs[game_proc]();
}

void ending_proc0(void);
void ending_proc1(void);
void ending_proc2(void);
void ending_proc3(void);
void ending_proc4(void);
void ending_proc5(void);

static SUB ending_procs[]={
	ending_proc0, ending_proc1, ending_proc2,
	ending_proc3, ending_proc4, ending_proc5
};

void main_sub6(void){
	ending_procs[sclear_proc]();
}

static int start_t, tile_bitno;
static int anim_proc;
void ending_proc0(void){
	stage=8;
	in_game=true;
	start_t =tile_bitno =0;
	anim_proc=0;
	long_delay =0x80;
	sclear_proc++;
}

void ending_proc1(void){
	static int taddrs[]={
		0x1000, 0x1400, 0x1800, 0x1c00,
		0x1010, 0x1410, 0x1810, 0x1c10
	};
	int taddr, i;

	msg700[msg700_ptr++]=2;
	msg700[msg700_ptr++]=0x20;
	msg700[msg700_ptr++]=0x1;
	taddr =taddrs[start_t] +tile_bitno;
	msg700[msg700_ptr++]=taddr>>8;
	msg700[msg700_ptr++]=taddr&0xff;
	for (i=0; i<0x20; i++){
		msg700[msg700_ptr++]=0;
	}
	msg700[msg700_ptr]=0;
	if (++start_t==8){
		if (++tile_bitno<16)
			start_t =0;
		else{
			gfx_setop(0,1);//清名字表
			long_delay =0x80;
			switch_bgvars();
			gfx_setting(0xc);
			sclear_proc++;
		}
	}
}

void ending_proc2(void){
	long_delay =0x80;
	sclear_proc++;
}

void ending_anim0(void);
void ending_anim1(void);
void ending_anim2(void);

SUB ending_anims[]={ending_anim0, ending_anim1, ending_anim2};

void ending_proc3(void){
	color_op();
	ending_anims[anim_proc]();
}

void ending_anim0(void){
	static unsigned char delay_x_ys[]={
		0x00,0x80,0x90,0x00,0x50,0x86,0xa8,0x60,0x8c,0xb4,0x98,0x8a,0xb8,0x70,0x94,0xd0,
		0x50,0x96,0xd3,0xa8,0x98,0xd6,0x78,0x94,0xdb,0x68,0x96,0xef,0x88,0x94,
	};
	int i,j;

	disp_forms[0xa+8]=0xcf;
	disp_forms[0xa+9]=0xc5;
	enemy[9].xspeed =-0x00a0;
	enemy[9].yspeed =-0x0090;
	for (i=9, j=0; i>=0; i--,j+=3){
		enemy[i].delay =delay_x_ys[j];
		chr_xs[0xa+i] =delay_x_ys[j+1];
		chr_ys[0xa+i] =delay_x_ys[j+2];
	}
	PLAYSOUND0(AIRPLANE_MOTOR);
	anim_proc++;
}

void ending_anim1(void){
	static unsigned char plane_forms[]={
		0xc5,0xc6,0xc7,0xc5,0xc6,0xc7,0xc5,0xc6,0xc7,0xc5,0xc8,0xc9,0xca,0xcb,
	};
	static unsigned char plane_loop_forms[]={0xcc,0xcd,0xce};
	static unsigned char bombing_forms[]={0x37,0x36,0x35,0x37,};
	static unsigned char msg_land_destroyed[]={
		0x01,0x0e,0x03,0x24,0x49,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x24,0x89,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x70,0x71,
		0x72,0x00,0x00,0x24,0xc9,0x7c,0x00,0x00,0x70,0x00,0x7c,0x74,0x7e,0x74,0x80,0x81,
		0x74,0x73,0x7f,
		0x01,0x06,0x01,0x30,0x32,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
		0x01,0x08,0x01,0x30,0x40,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
	};
	int i, j, delay;

	if (disp_forms[0xa+9]!=1){
		enemy[9].x_acum &=0xff;
		enemy[9].x_acum +=enemy[9].xspeed;
		chr_xs[0xa+9] +=enemy[9].x_acum>>8;
		enemy[9].y_acum &=0xff;
		enemy[9].y_acum +=enemy[9].yspeed;
		chr_ys[0xa+9] +=enemy[9].y_acum>>8;
		if (chr_ys[0xa+9]<0){
			disp_forms[0xa+9]=1;
		}else{
			enemy[9].xspeed +=2;
			const unsigned int plane_frame = (unsigned int)(enemy[9].delay >> 2);
			disp_forms[0xa+9] = plane_frame < SDL_arraysize(plane_forms)
				? plane_forms[plane_frame]
				: plane_loop_forms[(plane_frame - SDL_arraysize(plane_forms)) % SDL_arraysize(plane_loop_forms)];
			if (framecount &1)
				return;
			enemy[9].delay++;
		}
	}
	for (i=7; i>=0; i--){
		delay =enemy[i].delay;
		if (delay ==0 ){
			if (i==0){
				anim_proc++;
				return;
			}else
				enemy[i].delay =delay=0x100;
		}
		enemy[i].delay--;
		if (delay<0x60)
			disp_forms[0xa+i] =0;
		else if (delay<0x80){
			disp_forms[0xa+i] =bombing_forms[(delay>>3)&3];
		}else{
			if (delay==0x80){
				PLAYSOUND4(BOMBING2);
				if (i==3){
					disp_forms[0xa+8]=0;
					for (j=0; j<0x4b; j++){
						msg700[msg700_ptr++]=msg_land_destroyed[j];
					}
					msg700[msg700_ptr]=0;
				}
			}
			disp_forms[0xa+i] =0;
		}
	}//for (i=7; i>=0; i--)
}

static unsigned short msg_vaddr;
static int msg_no;
void ending_anim2(void){
	if (--long_delay==0){
		stop_bgm();
		msg_vaddr =0x27c0;
		msg_no =0;
		long_delay =0x280;
		gfx_setop(0,1);//清名字表
		PLAYSOUND0(ENDING);
		sclear_proc++;
	}
}

void ending_proc4(void){
	int no, msglen, blks_before, blks_after;
	unsigned char *pmsg;
	short vsc;

	if (framecount & 3)
		return;
	vsc=vscroll++;
	if (vscroll>=480)
		vscroll =0;
	if ((vsc&0xf)==0)
		no =msg_no++;
	else if ((vsc&0xf)==0x8)
		no=0;
	else
		return;
	pmsg =emsgs[no];
	if (pmsg==0){
		long_delay =0x230;
		sclear_proc++;
		return;
	}
	msglen =*pmsg++;
	blks_before =*pmsg++;
	blks_after =0x20 -blks_before-msglen;
	msg700[msg700_ptr++] =1;
	msg700[msg700_ptr++] =0x20;
	msg700[msg700_ptr++] =1;
	msg700[msg700_ptr++] =msg_vaddr>>8;
	msg700[msg700_ptr++] =msg_vaddr&0xff;
	while (blks_before--)
		msg700[msg700_ptr++] =0;
	while (msglen--)
		msg700[msg700_ptr++] =*pmsg++;
	while (blks_after--)
		msg700[msg700_ptr++] =0;
	msg700[msg700_ptr]=0;
	msg_vaddr +=0x40;
	if (msg_vaddr>=0x2f00)
		msg_vaddr =0x2000;
}

void ending_proc5(void){
	if (--long_delay==0){
		in_game =false;
		stage =0;
		sclear_proc =0;
		main_proc--;
	}
}

void init_stage(void){
	screen_page=0;
	screen_off=0;
	ntable_off600=0;
	a_addr_to_add=0x3000;
	if (!vstage_f){
		if (zstage_f){
			bright_count=0x10;
			update_pal_buf(0x10);
			cols_to_add=32;
		}else
			cols_to_add=48;
		cr_no_to_add=0;
		v_addr_to_add=0x2000;
	}else{
		rows_to_add=30+15;
		cr_no_to_add=0x1d;
		v_addr_to_add=0x2740;
	}

	add_blks600(0);
	add_bgs680(0);
}

void game_sub0(void){
	static const int bgno[]={0,6, 1,7, 2,6, 1,7, 3,6, 4,6, 0,6, 5,8};
	static const int bgno2[]={0,9, 1,7, 2,9, 1,7, 3,9, 4,9, 0,9, 5,9};
	unsigned char* pvars;
	int i;
	const int *pbgno = bossbgm ? bgno2 : bgno;

	gfx_setop(0,1);//清名字表
	write_msg(6);
	stage_clear=0;

	//stage=7;
	pvars=&stage_vars[stage<<5];
	zstage_f =pvars[0];
	vstage_f =pvars[1];
	page_of_bgswitch =pvars[8];
	for (i=0; i<3; i++)
		tjuge[i] =pvars[9+i];
	for (i=0; i<4; i++)
		bgcg3_spec[i] =pvars[12+i];
	for (i=0; i<8; i++)
		palcgs_spec[i] =pvars[16+i];
	screens_of_stage =pvars[24];
	through_solid =pvars[25];

	i=*((unsigned short*)&screens_blocks_data[stage<<1]);
	screens_blocks_table=&screens_blocks_data[i];
	i=*((unsigned short*)&screens_bg_data[stage<<1]);
	screens_bg_table=&screens_bg_data[i];
	i=*((unsigned short*)&stage_tiles_data[stage<<1]);
	tiles_of_blocks=&stage_tiles_data[i];
	i=*((unsigned short*)&stage_attrs_data[stage<<1]);
	attribs_of_blocks=&stage_attrs_data[i];

	set_bgm_tracks(pbgno[stage<<1], pbgno[(stage<<1)+1]);


	init_stage();
	init_stage_emy_jtables();
	game_proc++;
}

void msg_stageno_name(void){
	write_msg(0xc);
	msg700[msg700_ptr-2] =stage+'1';
	write_msg(stage+0x11);
}

void msg_p_rest(void){
	write_msg(0x7);
	const int rest = (player.go_flag ^ 1) + player.lives;
	if (rest <= 0){
		write_msg(0xf);
		return;
	}
	const int displayed = rest < 0x80 ? (rest < 99 ? rest : 99) : 0;
	msg700[msg700_ptr - 2] = '0' + displayed % 10;
	if (displayed >= 10)
		msg700[msg700_ptr - 3] = '0' + displayed / 10;
}

void game_sub1(void){
	msg_stageno_name();
	msg_p_rest();
	long_delay =0xc0;
	game_proc++;
}

void blink_score(int score){
	int i=5, tmp;
	int idx=msg700_ptr;

	if (framecount & 0x10){
		tmp=score;
		if (score){
			for (; i>0; i--,idx--){
				msg700[idx-4] =(tmp % 10)+'0';
				tmp /=10;
				if (!tmp)
					break;
			}
			msg700[msg700_ptr-3] ='0';
			msg700[msg700_ptr-2] ='0';
		}else{
			msg700[msg700_ptr-2] ='0';
		}
	}
}

void blink_scores(void){
	write_msg(0x9);
	blink_score(hi_score);
	write_msg(0xa);
	blink_score(player.score);
}

void game_sub2(void){
	static int stage_yoffs[]={-0x10, -8, 0, -8, -0x10, -0x10, -0x10, -0x10};
	
	if (--long_delay)
		blink_scores();
	else{
		gfx_setop(0,1);//清名字表
		gfx_setting(stage);
		update_pal_buf(0x20);
		vscroll =stage_yoffs[stage];
		play_bgm(0);
		in_game=true;
		game_proc++;
	}
}

void game_sub3(void){
	if (!initial_add_screen()){
		show_status=true;
		game_proc++;
		g_keys[SDL_SCANCODE_RIGHT]=0;
		g_keys[SDL_SCANCODE_LEFT]=0;
		g_keys[SDL_SCANCODE_DOWN]=0;
		g_keys[SDL_SCANCODE_UP]=0;
		g_keys[PLAYER_B]=0;
		g_keys[PLAYER_A]=0;
		g_keys[PLAYER_ST]=0;
		g_keys[PLAYER_SEL]=0;
		g_keys[PLAYER_RAPIDB]=0;
		g_keys[PLAYER_RAPIDA]=0;
	}
}

void game_sub4(void){
	check_pause();
	if (pause)
		return;
	if (stage_clear){
		game_proc=8;
		sclear_proc=0;
		return;
	}
	player_compre();
	if (player.go_flag){
		delay_before_go=0x60;
		game_proc=0xa;
		return;
	}
	scroll_addcolrow();
	enemy_proc();
	enemy_gen_scroll();
	enemy_gen_random();
	color_op();
	chang_boss_tiles();
}

void game_sub5(void){
	struct PLAYER player_sv=player;
	unsigned char stage_sv=stage;

	show_status=false;
	room_clear=0;
	clear_vars();
	stage =stage_sv;
	player.lives =player_sv.lives;
	player.go_flag =player_sv.go_flag;
	player.continues =player_sv.continues;
	player.bonus_score =player_sv.bonus_score;
	player.score =player_sv.score;

	if (stage_clear){
		player.gun_type[0] =player_sv.gun_type[0];
		player.gun_type[1] =player_sv.gun_type[1];
		player.crt_gun =player_sv.crt_gun;
		stage++;
		if (stage==8){
			long_delay =0x80;
			simple_f=0;
			num_of_allclears++;
			stage =9;
			game_proc=0;
			sclear_proc=0;
			main_proc++;
		}else{
			gfx_setting(0xa);
			vcontol=0;
			game_proc=0;
		}
	}else{
		gfx_setop(0,1);
		gfx_setting(0xa);
		write_msg(6);
		write_msg(0xd);
		PLAYSOUND0(GAMEOVER);
		if (player.continues){
			go_option=0;
			player.continues--;
			write_msg(0xe);
			game_proc++;
		}else
			game_proc=7;
	}
}

void game_sub6(void){
	if (player.triggers & 0x10){
		STOPSOUND(0);
		disp_forms[0]=0;
		if (go_option){
			g_keys[PLAYER_ST]=0;
			main_proc=0;
			long_delay=0x240;
			game_proc=0;
		}else{
			pre_game_init(false);
			game_proc=0;
		}
	}else{
		if (player.triggers & 0x20)
			go_option ^=1;
		disp_forms[0]=0xaa;
		chr_xs[0]=0x52;
		chr_ys[0]=0xa2 +(go_option<<4);
		blink_scores();
	}
}

void game_sub7(void){
	if (player.triggers & 0x10){
		STOPSOUND(0);
		disp_forms[0]=0;
		g_keys[PLAYER_ST]=0;
		main_proc=0;
		long_delay=0x240;
		game_proc=0;
	}else
		blink_scores();
}

void game_sub8(void){
	player_compre();
	if (player.go_flag){
		delay_before_go=0x60;
		game_proc=0xa;
	}else{
		scroll_addcolrow();
		enemy_proc();
		enemy_gen_scroll();
		color_op();
		chang_boss_tiles();
	}

	if (game_proc==0xa)
		return;
	if (long_delay ==0xffff)
		return;
	if (long_delay && --long_delay)
		return;
	if (player.state!=1)
		return;
	PLAYSOUND0(STAGE_CLEAR);
	game_proc++;
}

void game_sub9(void){
	stageclear_proc();
	player_compre();
	scroll_addcolrow();
	enemy_proc();
	color_op();
}

void game_suba(void){
	scroll_addcolrow();
	enemy_proc();
	enemy_gen_scroll();
	color_op();
	chang_boss_tiles();
	if (--delay_before_go==0){
		stage_clear=0;
		game_proc=5;
		in_game=false;
		stop_bgm();
	}
}
