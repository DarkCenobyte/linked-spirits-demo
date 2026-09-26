// ---- scene renderer (one program per world: S_LAB, S_CATH, S_SPACE) ----------
in vec2 q;
layout(location=0) out vec4 o;
layout(location=1) out float od;
layout(binding=2) uniform sampler2D TX;

vec3 gGaze=vec3(0,0,1);   // gaze direction (head space) of the eye being evaluated
mat3 hR;                  // head rotation (world -> head)
vec3 hP;                  // head position (world)
#define LIGHTS (TESTMODE?1.:U[12].x)
#define SYS U[12].y
#define TESTMODE (U[15].w>.5)

float eyeBall(vec3 e){return min(sph(e,ER),sph(e-gGaze*.006774,.0078));}

// ---------------------------------------------------------------- characters in the world
float android(vec3 p){
 float d=body(p,20),m=gM;
 vec3 h=hR*(p-hP);
 if(length(h)<.16){float f=face(h,U[6].w,U[7].x,U[7].y,0.);if(f<d){d=f;m=gM;}}
 else d=min(d,length(h)-.12);
 gM=m;
 return d;
}

#ifdef S_LAB
// ---------------------------------------------------------------- the white lab and its corridor
// room: x in [-4,4], y in [0,3.4], z in [-5,5]; door at z=5 leads to the corridor (z 5..60)
float pod(vec3 p){
 vec3 r=p-vec3(0,.85,-1.1);
 r.yz*=rot(-mix(.26,1.35,U[15].x));                     // tilt: 0 lying .. 1 upright
 float d=box(r-vec3(0,-.07,.1),vec3(.38,.05,1.05),.05);
 d=smin(d,box(r-vec3(0,-.1,.2),vec3(.3,.08,.8),.06),.04);
 d=min(d,cyl(p-vec3(0,.4,-1.1),.18,.42));
 d=min(d,cyl(p-vec3(0,.03,-1.1),.55,.03)-.02);
 return d;
}
float machines(vec3 p){
 vec3 r=p;r.x=abs(r.x);
 float d=box(r-vec3(1.6,.75,-2.),vec3(.35,.75,.3),.08);
 d=min(d,box(r-vec3(2.3,1.,-2.4),vec3(.25,1.,.25),.08));
 d=min(d,box(r-vec3(1.4,1.9,-2.3),vec3(.3,.2,.2),.06));
 return d;
}
float cables(vec3 p){
 vec3 r=p;r.x=abs(r.x);
 float d=1e3;
 for(int i=0;i<3;i++){float fi=float(i);
  vec3 a=vec3(1.3+fi*.12,.03,-1.9+fi*.1),b=vec3(.25,.3+fi*.1,-1.3);
  float h=clamp((r.x-b.x)/(a.x-b.x),0.,1.);
  vec3 c=mix(b,a,h);c.y=mix(b.y,a.y,sqrt(h));
  d=min(d,length(r-c)-.02);}
 return d;
}
float room(vec3 p){
 float d=min(p.y,3.4-p.y);
 d=min(d,4.-abs(p.x));
 d=min(d,p.z+5.);
 d=min(d,max(abs(p.z-5.)-.15,-box(p-vec3(0,1.4,5.),vec3(.85,1.4,1.),.02)));
 return d;
}
float corridor(vec3 p){
 float s=U[15].y,z=p.z-5.;
 vec3 r=p;
 r.xy*=rot(s*.015*max(z-10.,0.)*sin(z*.05));
 float wd=1.6+s*max(z-20.,0.)*.03,ht=3.+s*max(z-20.,0.)*.12;
 float d=min(wd-abs(r.x),ht-r.y);d=min(d,r.y);
 float rz=mod(z,2.)-1.;
 float rib=max(abs(rz)-.07,-(min(wd-.12-abs(r.x),ht-.12-r.y)));
 d=min(d,max(rib,-r.y));
 d=max(d,-(55.-z));
 return d;
}
float map(vec3 p){
 if(TESTMODE)return android(p);
 float d,m=1.;
 d=room(p);
 if(p.z>4.8){float c=corridor(p);d=p.z>5.2?c:max(d,c);}
 float x=pod(p);if(x<d){d=x;m=8.;}
 x=machines(p);if(x<d){d=x;m=8.;}
 x=cables(p);if(x<d){d=x;m=6.;}
 x=android(p);if(x<d){d=x;m=gM;}
 gM=m;
 return d;
}
vec3 env(vec3 d){
 vec3 c=vec3(.5,.52,.55)*(.3+.25*d.y)*LIGHTS;
 if(d.y>.15){vec2 u=d.xz/d.y;
  c+=vec3(3.,3.05,3.1)*(1.-smoothstep(0.,.06,max(abs(u.x+.35)-.28,abs(u.y-.2)-.5)))*LIGHTS;
  c+=vec3(2.4,2.45,2.5)*(1.-smoothstep(0.,.06,max(abs(u.x-.55)-.2,abs(u.y-.1)-.35)))*LIGHTS;}
 c+=vec3(.1,1.,.3)*smoothstep(.995,.999,dot(d,normalize(vec3(-.6,.1,.8))))*SYS;
 return c;
}
vec3 emis(vec3 p,vec3 n){
 vec3 e=vec3(0);
 if(TESTMODE)return e;
 if(p.y>3.39&&p.z<5.){vec2 g=abs(fract(p.xz/vec2(2.,2.5))-.5);
  e+=vec3(4.,4.05,4.1)*(1.-smoothstep(.28,.3,max(g.x,g.y)))*LIGHTS;}
 if(p.z>5.&&p.y<3.2){float rz=abs(mod(p.z-5.,2.)-1.);e+=vec3(2.,2.1,2.2)*(1.-smoothstep(.02,.04,rz))*step(.3,p.y)*LIGHTS;}
 vec3 r=p;r.x=abs(r.x);
 float g=length(r-vec3(1.6,1.3,-1.69))-.02;
 g=min(g,length(r-vec3(2.3,1.7,-2.14))-.02);
 e+=vec3(.2,2.,.6)*(1.-smoothstep(0.,.02,g))*SYS*6.;
 return e;
}
#endif
#ifndef S_LAB
float map(vec3 p){return android(p);}
vec3 env(vec3 d){return vec3(.5)*(.5+.5*d.y);}
vec3 emis(vec3 p,vec3 n){return vec3(0);}
#endif

// ---------------------------------------------------------------- shading helpers
vec3 nrm(vec3 p,float e){vec2 k=vec2(1,-1)*e;
 return normalize(k.xyy*map(p+k.xyy)+k.yyx*map(p+k.yyx)+k.yxy*map(p+k.yxy)+k.xxx*map(p+k.xxx));}
float softsh(vec3 p,vec3 l,float mx){float s=1.,t=.004;for(int i=0;i<48;i++){float h=map(p+l*t);s=min(s,12.*h/t);t+=clamp(h,.004,.2);if(s<.001||t>mx)break;}return clamp(s,0.,1.);}
float ao(vec3 p,vec3 n,float sc){float a=0.,w=1.;for(int i=1;i<6;i++){float h=sc*float(i);a+=w*(h-map(p+n*h));w*=.6;}return clamp(1.-a/(sc*3.),0.,1.);}

// iris: polar procedural stroma. r in [0,1] (limbus), a angle, pr pupil radius (fraction), col base colour
vec3 iris(float r,float a,float pr,vec3 col,out float isPupil){
 isPupil=0.;
 float bl=cos(mod(a,.698)-.349)/cos(.349);         // hidden nine-bladed diaphragm
 float prr=pr*mix(1.,bl,.12);
 float rr=clamp((r-prr)/(1.-prr),0.,1.);
 float w=a+.03*sin(rr*9.+a*5.)+.02*vn2(vec2(a*3.,rr*5.));
 float fib=vn2(vec2(w*90./PI,rr*2.5))*.5+vn2(vec2(w*190./PI,rr*5.))*.3+vn2(vec2(w*400./PI,rr*9.))*.2;
 fib=smoothstep(.2,.9,fib);
 float coll=.34+.07*(vn2(vec2(a*12./PI,2.))-.5);
 float crypt=smoothstep(.58,.8,vn2(vec2(w*16./PI,rr*5.)+7.3))*smoothstep(coll-.1,coll+.05,rr)*(1.-smoothstep(.45,.8,rr));
 float furrow=.5+.5*sin(rr*36.+vn2(vec2(a*4.,1.))*4.);
 vec3 c=col*(.35+.9*fib);
 c=mix(c,mix(col,vec3(.85,.6,.2),.6)*(.4+.8*fib),(1.-smoothstep(coll-.1,coll+.02,rr))*.7);
 c*=1.-.6*crypt;
 c*=1.-.18*smoothstep(.5,1.,rr)*smoothstep(.6,.9,furrow);
 c=mix(c,col*.08,smoothstep(.72,1.,rr));
 c*=.9+.1*sin(rr*300.);
 c*=mix(.25,1.,smoothstep(0.,.07,rr));
 if(r<prr){isPupil=1.;c=vec3(.004)+col*.01*smoothstep(prr*.8,prr,r);}
 return c;
}

vec3 shadeEye(vec3 p,vec3 rd,vec3 n,vec3 L,vec3 col,float pupil,float glow){
 vec3 hp=hR*(p-hP),hd=hR*rd,hn=hR*n,hl=hR*L;
 vec3 e=hp-gEyeC;
 float onC=dot(normalize(e),gGaze);
 vec3 c;
 float F=.03+.97*pow(1.-max(dot(-hd,hn),0.),5.);
 vec3 spec=env(transpose(hR)*reflect(hd,hn));
 if(onC>.8908){
  vec3 rr=refract(hd,hn,1./1.376);
  vec3 ic=gEyeC+gGaze*.0114;
  float t=dot(ic-hp,gGaze)/dot(rr,gGaze);
  vec3 ip=hp+rr*t-ic;
  vec3 ax=normalize(cross(gGaze,vec3(0,1,0))),ay=cross(ax,gGaze);
  float r=length(ip)/.006,a=atan(dot(ip,ay),dot(ip,ax)*gEyeSide),isP;
  vec3 ir=iris(r,a,pupil,col,isP);
  float lit=.25+.75*max(dot(gGaze,hl),0.);
  float caus=.6*pow(max(dot(normalize(ip+1e-6),-normalize(hl-gGaze*dot(hl,gGaze))),0.),3.)*smoothstep(.3,1.,r);
  c=ir*(lit+caus)+ir*glow*(1.-isP)*4.;
  if(r>1.)c=vec3(.8,.8,.82)*(.3+.7*max(dot(hn,hl),0.));
 }else c=vec3(.86,.85,.84)*(.35+.65*max(dot(hn,hl),0.))*mix(.75,1.,smoothstep(.2,.7,onC));
 {vec3 q=vec3(abs(e.x),e.yz);float au=mix(.5,-.2,U[6].w)-KU*q.x*q.x+7.*q.x,ae=atan(e.y,e.z);
  c*=mix(.3,1.,1.-smoothstep(-.18,.02,ae-au));
  c*=mix(.6,1.,smoothstep(-.42+KL*q.x*q.x+7.*q.x,-.25,ae));}
 return c*(1.-F)+spec*F*1.6;
}

// iris colour: green (identity) or heterochromia (left blue, right red)
vec3 irisCol(float side,float mode){
 return mix(vec3(.06,.36,.14),side>0.?vec3(.08,.2,.62):vec3(.62,.05,.04),mode);
}

vec3 shade(vec3 p,vec3 rd,float t,float m){
 vec3 n=nrm(p,max(t*.0004,.00005));
 vec3 L=normalize(vec3(-.3,.9,.35));
 float lt=TESTMODE?1.:LIGHTS;
 if(m==2.)return shadeEye(p,rd,n,L,irisCol(gEyeSide,U[7].z),U[6].z,U[7].w)*(.4+.6*lt);
 vec3 alb=vec3(.9,.9,.89);float rough=.35,met=0.;
 if(m==3.){alb=vec3(.55,.57,.6);met=1.;rough=.2;}
 if(m==4.)alb=vec3(.78,.66,.66);
 if(m==6.){alb=vec3(.12,.13,.14);rough=.3;}
 if(m==8.){alb=vec3(.92);rough=.25;}
 if(m<=4.){ // lash line on the upper lid margin, painted brows
  vec3 hp=hR*(p-hP),e=vec3(abs(hp.x),hp.yz)-EC;
  float bx=(abs(hp.x)-.031)/.019;
  if(hp.z>.05&&abs(bx)<1.)alb*=1.-.35*(1.-smoothstep(.0008,.0016,abs(hp.y-.022-.004*(1.-bx*bx)+.002*bx)))*smoothstep(1.,.7,abs(bx));
  float au=mix(.5,-.2,U[6].w)-KU*e.x*e.x+7.*e.x,ae=atan(e.y,e.z);
  if(length(e)<ER+.0032&&e.z>0.)alb*=mix(1.,.07,1.-smoothstep(.04,.1,abs(ae-au-.03)));
 }
 float sh=softsh(p+n*.002,L,3.),oc=ao(p,n,.004+t*.004);
 float wr=max(dot(n,L)+.35,0.)/1.35;
 vec3 h=normalize(L-rd);
 float sp=pow(max(dot(n,h),0.),2./(rough*rough))*(met>0.?2.:.4)*sh;
 float fr=.04+.96*pow(1.-max(dot(n,-rd),0.),5.);
 vec3 amb=vec3(.55,.57,.6)*(.6+.4*n.y)*oc;
 vec3 col=alb*(vec3(1.,.98,.95)*mix(wr,max(dot(n,L),0.)*sh,.7)*1.2+amb)*lt;
 col+=sp*vec3(1)*lt+env(reflect(rd,n))*fr*(met>0.?.9:.15)*oc;
 col+=emis(p,n);
 return col;
}

void main(){
 vec2 uv=(q*2.-1.)*vec2(2.353,1);
 vec3 cp=U[1].xyz,ta=U[2].xyz;
 vec3 fw=normalize(ta-cp),rt=normalize(cross(fw,vec3(0,1,0))),up=cross(rt,fw);
 rt=rt*cos(U[2].w)+up*sin(U[2].w);up=cross(rt,fw);
 vec3 rd=normalize(fw/tan(radians(U[1].w)*.5)+uv.x*rt+uv.y*up);
 hP=U[4].xyz;
 {float cy=cos(U[5].x),sy=sin(U[5].x),cx=cos(U[5].y),sx=sin(U[5].y);
  hR=transpose(mat3(cy,0,-sy,0,1,0,sy,0,cy)*mat3(1,0,0,0,cx,-sx,0,sx,cx));}
 gGaze=normalize(vec3(sin(U[6].x),sin(U[6].y),1));
 float t=0.,d;
 for(int i=0;i<220;i++){d=map(cp+rd*t);if(abs(d)<t*.0003+.00002||t>80.)break;t+=d*.9;}
 vec3 col=TESTMODE?vec3(.1,.105,.11)*(1.-.4*length(q-.5)):vec3(0);
 if(t<80.){
  vec3 p=cp+rd*t;float m=gM;
  col=shade(p,rd,t,m);
  if(p.y<.001&&m==1.&&!TESTMODE){      // polished floor: one reflection bounce
   vec3 r=reflect(rd,vec3(0,1,0));float t2=.01;
   for(int i=0;i<60;i++){d=map(p+r*t2);if(d<.002||t2>20.)break;t2+=d;}
   col=mix(col,t2<20.?shade(p+r*t2,r,t2,gM):vec3(0),.18);
  }
  od=t;
 }else od=1e4;
 o=vec4(col*exp2(U[3].z),1);
}
