#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

#define MAX_BODIES 100
#define FIXED_DT 0.016f

typedef struct {
    float x,y;
    float vx,vy;
    float mass;
    float size;
    SDL_Color color;
    bool dragging;
    int drag_offset_x, drag_offset_y;
} Body;

typedef struct {
    bool active;
    int bodyIndex;
    SDL_Rect rect;
} ContextMenu;

float gravity = 900.0f;
float restitution = 0.6f;
float friction = 0.8f;

SDL_Color random_color(){
    SDL_Color c = {rand()%256,rand()%256,rand()%256,255};
    return c;
}

bool AABB(Body *a, Body *b){
    return !(a->x + a->size < b->x ||
    a->x > b->x + b->size ||
    a->y + a->size < b->y ||
    a->y > b->y + b->size);
}

void positional_correction(Body *a, Body *b){
    float overlapX = fmin(a->x+a->size,b->x+b->size) - fmax(a->x,b->x);
    float overlapY = fmin(a->y+a->size,b->y+b->size) - fmax(a->y,b->y);

    if(overlapX < overlapY){
        float corr = overlapX/2;
        if(a->x < b->x){ a->x -= corr; b->x += corr; }
        else{ a->x += corr; b->x -= corr; }
    }else{
        float corr = overlapY/2;
        if(a->y < b->y){ a->y -= corr; b->y += corr; }
        else{ a->y += corr; b->y -= corr; }
    }
}

void resolve_collision(Body *a, Body *b){

    float nx = (a->x+a->size/2)-(b->x+b->size/2);
    float ny = (a->y+a->size/2)-(b->y+b->size/2);
    float dist = sqrtf(nx*nx+ny*ny);
    if(dist==0) return;

    nx/=dist; ny/=dist;

    float rvx = a->vx-b->vx;
    float rvy = a->vy-b->vy;
    float velAlongNormal = rvx*nx + rvy*ny;
    if(velAlongNormal>0) return;

    float invMass1=1/a->mass;
    float invMass2=1/b->mass;

    float j = -(1+restitution)*velAlongNormal;
    j/=invMass1+invMass2;

    float impulseX=j*nx;
    float impulseY=j*ny;

    a->vx+=impulseX*invMass1;
    a->vy+=impulseY*invMass1;
    b->vx-=impulseX*invMass2;
    b->vy-=impulseY*invMass2;

    positional_correction(a,b);
}

void draw_text(SDL_Renderer* r, TTF_Font* font,
               const char* txt, int x,int y){

    SDL_Color white={255,255,255,255};
    SDL_Surface* s=TTF_RenderText_Blended(font,txt,white);
    SDL_Texture* t=SDL_CreateTextureFromSurface(r,s);
    SDL_Rect dst={x,y,s->w,s->h};
    SDL_FreeSurface(s);
    SDL_RenderCopy(r,t,NULL,&dst);
    SDL_DestroyTexture(t);
               }

               int main(){

                   srand(time(NULL));
                   SDL_Init(SDL_INIT_VIDEO);
                   TTF_Init();

                   SDL_Window* window=SDL_CreateWindow(
                       "Physics Sandbox",
                       SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED,
                       1000,700,
                       SDL_WINDOW_RESIZABLE);

                   SDL_Renderer* renderer=
                   SDL_CreateRenderer(window,-1,
                                      SDL_RENDERER_ACCELERATED);

                   TTF_Font* font=
                   TTF_OpenFont("DejaVuSans.ttf",16);

                   if(!font){
                       printf("TTF_OpenFont failed: %s\n", TTF_GetError());
                       SDL_DestroyRenderer(renderer);
                       SDL_DestroyWindow(window);
                       TTF_Quit();
                       SDL_Quit();
                       return 1;
                   }

                   int winW=1000,winH=700;

                   Body bodies[MAX_BODIES];
                   int bodyCount=5;

                   for(int i=0;i<bodyCount;i++){
                       bodies[i].x=200+i*70;
                       bodies[i].y=50;
                       bodies[i].vx=0;
                       bodies[i].vy=0;
                       bodies[i].size=50;
                       bodies[i].mass=50;
                       bodies[i].color=random_color();
                       bodies[i].dragging=false;
                   }

                   ContextMenu menu={false,-1,{0,0,200,180}};
                   bool running=true;
                   SDL_Event e;

                   while(running){

                       while(SDL_PollEvent(&e)){

                           if(e.type==SDL_QUIT) running=false;

                           if(e.type==SDL_WINDOWEVENT &&
                               e.window.event==SDL_WINDOWEVENT_SIZE_CHANGED){
                               winW=e.window.data1;
                           winH=e.window.data2;
                               }

                               // SPAWN (środkowy klik)
                               if(e.type==SDL_MOUSEBUTTONDOWN &&
                                   e.button.button==SDL_BUTTON_MIDDLE){

                                   if(bodyCount<MAX_BODIES){
                                       bodies[bodyCount].x=e.button.x;
                                       bodies[bodyCount].y=e.button.y;
                                       bodies[bodyCount].vx=0;
                                       bodies[bodyCount].vy=0;
                                       bodies[bodyCount].size=50;
                                       bodies[bodyCount].mass=50;
                                       bodies[bodyCount].color=random_color();
                                       bodies[bodyCount].dragging=false;
                                       bodyCount++;
                                   }
                                   }

                                   // --- MENU KLIK (ma pierwszeństwo) ---
                                   if(e.type==SDL_MOUSEBUTTONDOWN &&
                                       e.button.button==SDL_BUTTON_LEFT &&
                                       menu.active){

                                       int relY=e.button.y-menu.rect.y;
                                   Body* b=&bodies[menu.bodyIndex];

                                   if(relY<30) b->color=random_color();
                                   else if(relY<60) b->mass+=10;
                                   else if(relY<90 && b->mass>10) b->mass-=10;
                                   else if(relY<120) b->size+=10;
                                   else if(relY<150 && b->size>20) b->size-=10;
                                   else if(relY<180){
                                       bodies[menu.bodyIndex]=bodies[bodyCount-1];
                                       bodyCount--;
                                   }

                                   menu.active=false;
                                       }
                                       // --- PRZECIĄGANIE ---
                                       else if(e.type==SDL_MOUSEBUTTONDOWN &&
                                           e.button.button==SDL_BUTTON_LEFT){

                                           for(int i=0;i<bodyCount;i++){
                                               if(e.button.x>=bodies[i].x &&
                                                   e.button.x<=bodies[i].x+bodies[i].size &&
                                                   e.button.y>=bodies[i].y &&
                                                   e.button.y<=bodies[i].y+bodies[i].size){

                                                   bodies[i].dragging=true;
                                               bodies[i].drag_offset_x=
                                               e.button.x-bodies[i].x;
                                               bodies[i].drag_offset_y=
                                               e.button.y-bodies[i].y;
                                               bodies[i].vx=0;
                                               bodies[i].vy=0;
                                                   }
                                           }
                                           }

                                           if(e.type==SDL_MOUSEBUTTONUP &&
                                               e.button.button==SDL_BUTTON_LEFT){
                                               for(int i=0;i<bodyCount;i++)
                                                   bodies[i].dragging=false;
                                               }

                                               // --- PRAWY KLIK (menu nad klockiem) ---
                                               if(e.type==SDL_MOUSEBUTTONDOWN &&
                                                   e.button.button==SDL_BUTTON_RIGHT){

                                                   for(int i=0;i<bodyCount;i++){
                                                       if(e.button.x>=bodies[i].x &&
                                                           e.button.x<=bodies[i].x+bodies[i].size &&
                                                           e.button.y>=bodies[i].y &&
                                                           e.button.y<=bodies[i].y+bodies[i].size){

                                                           menu.active=true;
                                                       menu.bodyIndex=i;

                                                       menu.rect.x =
                                                       (int)(bodies[i].x +
                                                       bodies[i].size/2 -
                                                       menu.rect.w/2);

                                                       menu.rect.y =
                                                       (int)(bodies[i].y -
                                                       menu.rect.h - 5);

                                                       if(menu.rect.x < 0)
                                                           menu.rect.x = 0;

                                                           if(menu.rect.x + menu.rect.w > winW)
                                                               menu.rect.x = winW - menu.rect.w;

                                                           if(menu.rect.y < 0)
                                                               menu.rect.y =
                                                               (int)(bodies[i].y +
                                                               bodies[i].size + 5);

                                                           break;
                                                           }
                                                   }
                                                   }
                       }

                       int mx,my;
                       SDL_GetMouseState(&mx,&my);

                       // --- FIZYKA ---
                       for(int i=0;i<bodyCount;i++){

                           Body* b=&bodies[i];

                           if(b->dragging){

                               float newX=mx-b->drag_offset_x;
                               float newY=my-b->drag_offset_y;

                               b->vx=(newX-b->x)/FIXED_DT;
                               b->vy=(newY-b->y)/FIXED_DT;

                               b->x=newX;
                               b->y=newY;
                               continue;
                           }

                           b->vy+=gravity*FIXED_DT;
                           b->x+=b->vx*FIXED_DT;
                           b->y+=b->vy*FIXED_DT;

                           if(b->x<0){ b->x=0; b->vx*=-restitution; }
                           if(b->x+b->size>winW){
                               b->x=winW-b->size;
                               b->vx*=-restitution;
                           }
                           if(b->y<0){ b->y=0; b->vy*=-restitution; }
                           if(b->y+b->size>winH){
                               b->y=winH-b->size;
                               b->vy*=-restitution;
                               b->vx*=friction;
                           }
                       }

                       for(int i=0;i<bodyCount;i++)
                           for(int j=i+1;j<bodyCount;j++)
                               if(AABB(&bodies[i],&bodies[j]))
                                   resolve_collision(&bodies[i],&bodies[j]);

                       // --- RENDER ---
                       SDL_SetRenderDrawColor(renderer,20,20,20,255);
                       SDL_RenderClear(renderer);

                       for(int i=0;i<bodyCount;i++){
                           SDL_SetRenderDrawColor(renderer,
                                                  bodies[i].color.r,
                                                  bodies[i].color.g,
                                                  bodies[i].color.b,255);

                           SDL_Rect r={(int)bodies[i].x,
                               (int)bodies[i].y,
                               (int)bodies[i].size,
                               (int)bodies[i].size};

                               SDL_RenderFillRect(renderer,&r);
                       }

                       if(menu.active){
                           SDL_SetRenderDrawColor(renderer,40,40,40,255);
                           SDL_RenderFillRect(renderer,&menu.rect);

                           draw_text(renderer,font,"Kolor",
                                     menu.rect.x+10,menu.rect.y+5);
                           draw_text(renderer,font,"Masa +",
                                     menu.rect.x+10,menu.rect.y+35);
                           draw_text(renderer,font,"Masa -",
                                     menu.rect.x+10,menu.rect.y+65);
                           draw_text(renderer,font,"Rozmiar +",
                                     menu.rect.x+10,menu.rect.y+95);
                           draw_text(renderer,font,"Rozmiar -",
                                     menu.rect.x+10,menu.rect.y+125);
                           draw_text(renderer,font,"Usun",
                                     menu.rect.x+10,menu.rect.y+155);
                       }

                       SDL_RenderPresent(renderer);
                       SDL_Delay(16);
                   }

                   TTF_CloseFont(font);
                   SDL_DestroyRenderer(renderer);
                   SDL_DestroyWindow(window);
                   TTF_Quit();
                   SDL_Quit();
                   return 0;
               }
