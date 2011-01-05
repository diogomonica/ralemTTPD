#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define MAXLEN 512

char *parseName(char *name);
int parseDay(char *day);
int parseMonth(char *month);
int fileExists(char *fileFrom);
char *identifyType(int day,int month);

typedef struct signs{
  int day;
  char *sign;
}signs;

signs signo[] = 
  {{20,"Aquarius"},
   {50,"Pisces"},
   {81,"Aries"},
   {111,"Taurus"},
   {142,"Gemini"},
   {174,"Cancer"},
   {205,"Leo"},
   {236,"Virgo"},
   {267,"Libra"},
   {298,"Scorpio"},
   {327,"Sagittarius"},
   {357,"Capricorn"}};

short nDays[]={0, 31,60,91,121,152,182,213,244,274,305,335};

int main(){
  int day,month,len=0, cookieN=0;
  int retorno;
  char *data,*name,*temp,*cookie,times[MAXLEN];
  char *cType=getenv("CONTENT_TYPE");
  char *cLen=getenv("CONTENT_LENGTH");
  
  
  if((cType==NULL) || (cLen==NULL))
    return -1;
  
  len=atoi(cLen);
  
  data=malloc(len+1);
  name=malloc(len);
  fgets(data,len+1,stdin);
  retorno=sscanf(data,"n=%[^&]&d=%d+%d",name,&day,&month);
  
  if(retorno!=3||day>31||month>12){
    printf("Content-Type: text/html\r\n\r\n%s","<html><head><title>Zodiac</title></head><body size=20>Fill the Name and Date of birth correctly!</body></html>");
    exit(0);
  }
  
  if((cookie=getenv("HTTP_COOKIE"))==NULL){
    cookieN=0;
  }else{
    temp=strrchr(cookie,'=');
    temp++;
    cookieN=atoi(temp);
  }
  
  name=parseName(name);
  temp=identifyType(day,month);    
  
  memset(times,'0',sizeof(times));
  snprintf(times,sizeof(times),"<html><head><title>Zodiac</title></head><body align=center size=25>Dear, %s, your zodiac sign is: %s\n<br>Form accessed %d times</body></html>\n",name,temp,cookieN+1);
  printf("Set-Cookie: VIEWS=%d; path=/\r\nContent-Type: text/html\r\n\r\n%s",++cookieN, times);

  free(name);
  free(data);
  return 0;
  
}


char *parseName(char *name){
  char *ret;
  while((ret=strchr(name,'+'))!=NULL)
    *ret=' ';
  return name;
}

char *identifyType(int day,int month){
  int i=signo[0].day,j=0,yearDay=nDays[month-1]+day;
   while (i<=yearDay){
     i=signo[++j].day;
   }
   if(j==0)
     j=12;
   return signo[j-1].sign;
}
