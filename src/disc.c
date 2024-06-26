/*Elkulator v1.0 by Sarah Walker
  Disc support (also some tape)*/
  
#include <allegro.h>
#include <stdio.h>
#include <string.h>
#include "elk.h"

void (*fdccallback)();
void (*fdcdata)(uint8_t dat);
void (*fdcspindown)();
void (*fdcfinishread)();
void (*fdcnotfound)();
void (*fdcdatacrcerror)();
void (*fdcheadercrcerror)();
void (*fdcwriteprotect)();
int  (*fdcgetdata)(int last);

struct drives drives[2];
int curdrive;

int discchanged[2]={0,0};

struct
{
        char *ext;
        void (*load)(int drive, char *fn);
        void (*close)(int drive);
        int size;
}
loaders[]=
{
        {"SSD",ssd_load,ssd_close,  80*10*256},
        {"DSD",dsd_load,ssd_close,2*80*10*256},
        {"ADF",adf_load,adf_close,  80*16*256},
        {"ADL",adl_load,adf_close,2*80*16*256},
        {"FDI",fdi_load,fdi_close,-1},
        {0,0,0}
};

int driveloaders[2];

void loaddisc(int drive, char *fn)
{
        int c=0;
        char *p;
        FILE *f;
        setejecttext(drive,"");
        if (!fn)
        {
                rpclog("!fn\n");
                return;
        }
        p=get_extension(fn);
        if (!p)
        {
                rpclog("!p\n");
                return;
        }
        setejecttext(drive,fn);
        rpclog("Loading :%i %s %s\n",drive,fn,p);
        while (loaders[c].ext)
        {
                if (!strcasecmp(p,loaders[c].ext))
                {
                        driveloaders[drive]=c;
                        loaders[c].load(drive,fn);
                        return;
                }
                c++;
        }
        printf("Couldn't load %s %s\n",fn,p);
        /*No extension match, so guess based on image size*/
        f=fopen(fn,"rb");
        if (!f) return;
        fseek(f,-1,SEEK_END);
        c=ftell(f)+1;
        fclose(f);
        rpclog("Size %i\n",c);
        if (c==(800*1024)) /*800k ADFS/DOS - 80*2*5*1024*/
        {
                driveloaders[drive]=2;
                loaders[2].load(drive,fn);
                return;
        }
        if (c==(640*1024)) /*640k ADFS/DOS - 80*2*16*256*/
        {
                driveloaders[drive]=3;
                loaders[3].load(drive,fn);
                return;
        }
        if (c==(720*1024)) /*720k DOS - 80*2*9*512*/
        {
                driveloaders[drive]=3;
                adl_loadex(drive,fn,9,512,0);
                return;
        }
        if (c==(360*1024)) /*360k DOS - 40*2*9*512*/
        {
                driveloaders[drive]=3;
                adl_loadex(drive,fn,9,512,1);
                return;
        }
        if (c<=(200*1024)) /*200k DFS - 80*1*10*256*/
        {
                driveloaders[drive]=0;
                loaders[0].load(drive,fn);
                return;
        }
        if (c<=(400*1024)) /*400k DFS - 80*2*10*256*/
        {
                driveloaders[drive]=1;
                loaders[1].load(drive,fn);
                return;
        }
}

void newdisc(int drive, char *fn)
{
        int c=0,d;
        FILE *f;
        char *p=get_extension(fn);
        while (loaders[c].ext)
        {
                if (!strcasecmp(p,loaders[c].ext) && loaders[c].size!=-1)
                {
                        f=fopen(fn,"wb");
                        for (d=0;d<loaders[c].size;d++) putc(0,f);
                        if (!strcasecmp(p,"ADF"))
                        {
                                fseek(f,0,SEEK_SET);
                                putc(7,f);
                                fseek(f,0xFD,SEEK_SET);
                                putc(5,f); putc(0,f); putc(0xC,f); putc(0xF9,f); putc(0x04,f);
                                fseek(f,0x1FB,SEEK_SET);
                                putc(0x88,f); putc(0x39,f); putc(0,f); putc(3,f); putc(0xC1,f);
                                putc(0,f); putc('H',f); putc('u',f); putc('g',f); putc('o',f);
                                fseek(f,0x6CC,SEEK_SET);
                                putc(0x24,f);
                                fseek(f,0x6D6,SEEK_SET);
                                putc(2,f); putc(0,f); putc(0,f); putc(0x24,f);
                                fseek(f,0x6FB,SEEK_SET);
                                putc('H',f); putc('u',f); putc('g',f); putc('o',f);
                        }
                        if (!strcasecmp(p,"ADL"))
                        {
                                fseek(f,0,SEEK_SET);
                                putc(7,f);
                                fseek(f,0xFD,SEEK_SET);
                                putc(0xA,f); putc(0,f); putc(0x11,f); putc(0xF9,f); putc(0x09,f);
                                fseek(f,0x1FB,SEEK_SET);
                                putc(0x01,f); putc(0x84,f); putc(0,f); putc(3,f); putc(0x8A,f);
                                putc(0,f); putc('H',f); putc('u',f); putc('g',f); putc('o',f);
                                fseek(f,0x6CC,SEEK_SET);
                                putc(0x24,f);
                                fseek(f,0x6D6,SEEK_SET);
                                putc(2,f); putc(0,f); putc(0,f); putc(0x24,f);
                                fseek(f,0x6FB,SEEK_SET);
                                putc('H',f); putc('u',f); putc('g',f); putc('o',f);
                        }
                        fclose(f);
                        loaddisc(drive,fn);
                        return;
                }
                c++;
        }
}

/*void loadtape(char *fn)
{
        char *p;
        if (!fn) return;
//        if (fn[0]==0) return;
        p=get_extension(fn);
        if (!p) return;
        if (p[0]=='u' || p[0]=='U') openuef(fn);
        else                        opencsw(fn);
}*/

void closedisc(int drive)
{
        if (loaders[driveloaders[drive]].close) loaders[driveloaders[drive]].close(drive);
}

int disc_notfound=0;

void disc_reset()
{
        drives[0].poll=drives[1].poll=0;
        drives[0].seek=drives[1].seek=0;
        drives[0].readsector=drives[1].readsector=0;
        curdrive=0;
}

void disc_poll()
{
        if (drives[curdrive].poll) drives[curdrive].poll();
        if (disc_notfound)
        {
                disc_notfound--;
                if (!disc_notfound)
                   fdcnotfound();
        }
}

int oldtrack[2]={0,0};
void disc_seek(int drive, int track)
{
        if (drives[drive].seek)
           drives[drive].seek(drive,track);
        ddnoise_seek(track-oldtrack[drive]);
//fdctime=200;
        oldtrack[drive]=track;
}

void disc_readsector(int drive, int sector, int track, int side, int density)
{
        if (drives[drive].readsector)
           drives[drive].readsector(drive,sector,track,side,density);
        else
           disc_notfound=10000;
}

void disc_writesector(int drive, int sector, int track, int side, int density)
{
        if (drives[drive].writesector)
           drives[drive].writesector(drive,sector,track,side,density);
        else
           disc_notfound=10000;
}

void disc_readaddress(int drive, int track, int side, int density)
{
        if (drives[drive].readaddress)
           drives[drive].readaddress(drive,track,side,density);
        else
           disc_notfound=10000;
}

void disc_format(int drive, int track, int side, int density)
{
        if (drives[drive].format)
           drives[drive].format(drive,track,side,density);
        else
           disc_notfound=10000;
}


void loadtape(char *fn)
{
        char *p;
        if (!fn) return;
//        if (fn[0]==0) return;
        p=get_extension(fn);
        if (!p) return;
        if (p[0]=='u' || p[0]=='U') openuef(fn);
        else                        opencsw(fn);
}
