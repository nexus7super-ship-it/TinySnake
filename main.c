// MIT License

// Copyright (c) 2026 nexus7super-ship-it

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
extern void free(void*);typedef struct{int x,y;}P;static uint32_t z=123;int r(){return(z=z*1103515245+12345)%20;}int main(){ xcb_connection_t* c=xcb_connect(0,0); const xcb_setup_t* s=xcb_get_setup(c); xcb_screen_t* sc=xcb_setup_roots_iterator(s).data; xcb_window_t w=xcb_generate_id(c); uint32_t m=XCB_CW_BACK_PIXEL|XCB_CW_EVENT_MASK; uint32_t v[]={sc->black_pixel,XCB_EVENT_MASK_KEY_PRESS}; xcb_create_window(c,XCB_COPY_FROM_PARENT,w,sc->root,0,0,400,400,0,XCB_WINDOW_CLASS_INPUT_OUTPUT,sc->root_visual,m,v); xcb_change_property(c,XCB_PROP_MODE_REPLACE,w,XCB_ATOM_WM_NAME,XCB_ATOM_STRING,8,9,"TinySnake"); xcb_size_hints_t h; h.flags=XCB_ICCCM_SIZE_HINT_P_MIN_SIZE|XCB_ICCCM_SIZE_HINT_P_MAX_SIZE; h.min_width=h.max_width=h.min_height=h.max_height=400; xcb_change_property(c,XCB_PROP_MODE_REPLACE,w,XCB_ATOM_WM_NORMAL_HINTS,XCB_ATOM_WM_SIZE_HINTS,32,sizeof(h)/4,&h); xcb_map_window(c,w); xcb_gcontext_t g=xcb_generate_id(c); xcb_create_gc(c,g,w,0,0); P sn[400]; int l=3,dx=0,dy=-1,i; for(i=0;i<l;++i)sn[i]=(P){10,10+i}; P f={5,5}; for(;;){ xcb_generic_event_t* e; while((e=xcb_poll_for_event(c))){ if((e->response_type&0x7f)==XCB_KEY_PRESS){ uint8_t k=((xcb_key_press_event_t*)e)->detail; if(k==9)return 0;  if(k==113&&!dx){dx=-1;dy=0;}  if(k==114&&!dx){dx=1;dy=0;}  if(k==111&&!dy){dx=0;dy=-1;}  if(k==116&&!dy){dx=0;dy=1;}  } free(e); } P next={sn[0].x+dx,sn[0].y+dy}; if(next.x<0||next.x>=20||next.y<0||next.y>=20)break;  for(i=0;i<l;++i)if(sn[i].x==next.x&&sn[i].y==next.y)return 0;  if(next.x==f.x&&next.y==f.y){ l++; f=(P){r(),r()}; } for(i=l-1;i>0;--i)sn[i]=sn[i-1]; sn[0]=next; uint32_t cv[]={0};  xcb_change_gc(c,g,XCB_GC_FOREGROUND,cv); xcb_rectangle_t rt={0,0,400,400}; xcb_poly_fill_rectangle(c,w,g,1,&rt); cv[0]=0xff0000;  xcb_change_gc(c,g,XCB_GC_FOREGROUND,cv); xcb_rectangle_t fr={f.x*20,f.y*20,19,19}; xcb_poly_fill_rectangle(c,w,g,1,&fr); cv[0]=0x00ff00;  xcb_change_gc(c,g,XCB_GC_FOREGROUND,cv); for(i=0;i<l;++i){ xcb_rectangle_t sr={sn[i].x*20,sn[i].y*20,19,19}; xcb_poly_fill_rectangle(c,w,g,1,&sr); } xcb_flush(c); volatile int t; for(t=0;t<25000000;++t);  } return 0;}
