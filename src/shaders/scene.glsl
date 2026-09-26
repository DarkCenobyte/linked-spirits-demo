// ---- scene renderer (one program per world: S_LAB, S_CATH, S_SPACE) ----------
in vec2 q;
layout(location=0) out vec4 o;
layout(location=1) out float od;
layout(binding=2) uniform sampler2D TX;

vec3 gGaze=vec3(0,0,1);   // gaze direction (head space) of the eye being evaluated
vec3 aGaze,cGaze;         // android / cyborg gaze
float gWho;               // 0 android, 1 cyborg (last character hit)
mat3 cR;vec3 cP;          // cyborg head frame
vec2 gBlink;              // blink (right, left) of the character being shaded
mat3 hR,aR;               // head rotation (world -> head): current shading frame, android
vec3 hP,aP;               // head position (world)
#define LIGHTS (TESTMODE?1.:U[12].x)
#define SYS U[12].y
#define TESTMODE (U[15].w>.5)
bool gCharOnly=false;   // shadow rays that only see the characters

float eyeBall(vec3 e){return min(sph(e,ER),sph(e-gGaze*.006774,.0078));}

// ---------------------------------------------------------------- characters in the world
float android(vec3 p){
 float d=body(p,20),m=gM;
 vec3 h=aR*(p-aP);
 if(length(h)<.16){gGaze=aGaze;float f=face(h,vec2(U[6].w),U[7].x,U[7].y,0.);if(f<d){d=f;m=gM;}}
 else d=min(d,length(h)-.12);
 gM=m;gWho=0.;
 return d;
}

#ifdef S_LAB
// ---------------------------------------------------------------- the white lab and its corridor
// room: x in [-4,4], y in [0,3.4], z in [-5,5]; door at z=5 leads to the corridor (z 5..60)
float pod(vec3 p){
 // a reclined cradle hinged at her pelvis; U[15].x = recline angle
 vec3 r=p-U[19].xyz;float a=U[15].x,c=cos(a),sn=sin(a);
 r.yz=mat2(c,sn,-sn,c)*r.yz;
 float d=box(r-vec3(0,-.1,-.19),vec3(.3,.86,.05),.05);
 d=min(d,box(r-vec3(0,-.96,.02),vec3(.2,.025,.14),.02));        // foot rest
 d=smin(d,box(r-vec3(0,.62,-.1),vec3(.12,.1,.04),.03),.04);      // head cradle
 d=min(d,cap(r,vec3(0,-.1,-.25),vec3(0,-.1,-.5),.06));            // arm to the column
 vec3 cq=p-vec3(U[19].x,0,U[19].z-.35-.5*sn);
 d=min(d,cyl(cq-vec3(0,.45,0),.1,.45));
 d=min(d,cyl(cq-vec3(0,.02,0),.45,.02)-.01);
 return d;
}
float machines(vec3 p){
 vec3 r=p;r.x=abs(r.x);
 float d=box(r-vec3(1.7,.9,-1.9),vec3(.28,.9,.26),.1);           // near pillars
 d=min(d,box(r-vec3(2.5,1.2,-2.6),vec3(.3,1.2,.3),.12));          // far pillars
 d=min(d,box(r-vec3(1.25,2.3,-2.3),vec3(.35,.08,.25),.06));       // hanging arm
 d=min(d,cap(r,vec3(1.25,2.3,-2.3),vec3(1.7,3.9,-2.3),.035));
 return d;
}
float cables(vec3 p){
 vec3 r=p;r.x=abs(r.x);
 float d=1e3;
 for(int i=0;i<3;i++){float fi=float(i);
  vec3 a=vec3(1.45,.025,-1.75+fi*.09),b=vec3(.35,.12,-1.3-fi*.08);
  float h=clamp((r.x-b.x)/(a.x-b.x),0.,1.);
  vec3 c=mix(b,a,h);c.y=mix(b.y,a.y,sqrt(h))+.012*fi;c.z+=.1*sin(h*3.1)*(fi-1.);
  d=min(d,length(r-c)-.018);}
 return d;
}
// air of the white room (rounded "infinity cove") + corridor; the solid is its complement
float airRoom(vec3 p){return box(p-vec3(0,2.,.5),vec3(5.,2.,6.),.7);}
float airCorr(vec3 p){
 float s=U[15].y,z=p.z-6.;
 vec3 r=p;
 r.xy=rot(s*.012*max(z-12.,0.)*sin(z*.045)*(1.-smoothstep(40.,50.,z)))*(r.xy-vec2(0,1.5))+vec2(0,1.5);   // it slowly twists
 float wd=1.5+s*max(z-18.,0.)*.035,ht=3.+s*max(z-18.,0.)*.14;
 float a=box(vec3(r.x,r.y-ht*.5,z-27.),vec3(wd,ht*.5,33.2),.3);
 float rz=abs(mod(z,2.4)-1.2)-.09;
 float rib=max(max(rz,-box(vec3(r.x,r.y-ht*.5,0),vec3(wd-.14,ht*.5-.14,1.),.25)),z-51.);
 return max(a,-rib)*(1.-.45*s);    // the twist bends the field: keep it conservative
}
float doors(vec3 p){
 float s=U[15].y,wd=1.5+s*1.19,ht=3.+s*4.76,a=U[12].w*1.35;
 vec3 r=p-vec3(0,0,58.);
 vec3 l=r+vec3(wd,0,0);l.xz=rot(-a)*l.xz;
 float d=box(l-vec3(wd*.5,ht*.5,.07),vec3(wd*.5-.006,ht*.5,.07),.02);
 vec3 k=r-vec3(wd,0,0);k.xz=rot(a)*k.xz;
 d=min(d,box(k+vec3(wd*.5,-ht*.5,-.07),vec3(wd*.5-.006,ht*.5,.07),.02));
 return d;
}
float racks(vec3 p){                       // behind the glass partition
 vec3 r=p-vec3(-4.35,1.3,0.);r.z=mod(r.z+.9,1.8)-.9;
 return box(r,vec3(.35,1.3,.7),.03);
}
float map(vec3 p){
 if(TESTMODE||gCharOnly)return android(p);
 float d,m=1.;
 d=-min(airRoom(p),airCorr(p));
 float x=box(p-vec3(-3.25,2.,-.5),vec3(.03,2.,.03),0.);            // glass mullions
 x=min(x,box(vec3(p.x+3.25,p.y-2.,mod(p.z+.5,2.4)-1.2),vec3(.03,2.,.03),0.));
 x=max(x,abs(p.z)-4.5);
 if(x<d){d=x;m=3.;}
 x=racks(p);if(x<d){d=x;m=9.;}
 if(p.z>50.){x=doors(p);if(x<d){d=x;m=8.;}}
 x=pod(p);if(x<d){d=x;m=8.;}
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
 c+=vec3(.1,1.,.3)*smoothstep(.995,.999,dot(d,normalize(vec3(-.6,.1,.8))))*min(SYS,1.);
 return c;
}
vec3 emis(vec3 p,vec3 n,float m){
 vec3 e=vec3(0);
 if(TESTMODE)return e;
 if(p.y>3.98&&p.z<6.){vec2 g=abs(fract((p.xz+vec2(0,.5))/vec2(2.5,3.))-.5);
  e+=vec3(3.,3.05,3.1)*(1.-smoothstep(.3,.32,max(g.x,g.y)))*LIGHTS;}
 e+=vec3(4.,4.1,4.3)*(1.-smoothstep(.0,.02,tor(p-vec3(0,3.95,-1.2),1.3,.04)))*LIGHTS;          // halo above the pod
 if(p.z>58.2)e+=vec3(9.,9.2,9.5)*(.2+U[12].w*2.);
 if(p.z>6.&&p.z<57.){float rz=abs(mod(p.z-6.,2.4)-1.2);e+=vec3(2.,2.1,2.2)*(1.-smoothstep(.03,.05,rz))*step(.2,p.y)*LIGHTS;}
 if(p.x<-4.){vec2 g=fract(vec2(p.z*8.,p.y*12.));float on=step(.6,h2(floor(vec2(p.z*8.,p.y*12.))+floor(U[0].x*3.)*.37));
  e+=vec3(.1,1.,.35)*on*(1.-smoothstep(.1,.2,length(g-.5)))*3.*step(-4.01,-p.x);}
 vec3 r=p;r.x=abs(r.x);float sd=p.x<0.?0.:1.;
 float g=length(r-vec3(1.7,1.45,-1.63))-.018;
 e+=vec3(.2,2.,.6)*(1.-smoothstep(0.,.02,g))*step(sd*2.+.5,SYS)*6.;
 g=length(r-vec3(2.5,2.1,-2.29))-.018;
 e+=vec3(.2,2.,.6)*(1.-smoothstep(0.,.02,g))*step(sd*2.+1.5,SYS)*6.;
 // screen glyphs on the near pillars
 if(abs(r.z+1.63)<.02&&abs(r.x-1.7)<.2&&r.y>.9&&r.y<1.35){vec2 u=vec2(r.x*40.,r.y*30.);
  e+=vec3(.1,.8,.3)*step(.55,h2(floor(u)+floor(U[0].x*2.)))*step(.3,fract(u.y))*step(3.5,SYS);}
 return e;
}
#endif
#ifdef S_CATH
// ---------------------------------------------------------------- the biomechanical cathedral
float dots(vec3 p,float r){vec3 q=fract(p*14.)-.5;return (length(q)-r)/14.;}
// nave along +z; she enters at z=0; the cyborg's face waits at the far wall (0,2.1,40)
#define BREATH (.5+.5*sin(U[0].x*1.309))
float cyborg(vec3 p){
 vec3 h=cR*(p-cP);
 if(length(h)>.17)return length(h)-.14;
 gGaze=cGaze;
 return face(h,U[9].zw,U[10].w,U[11].x,1.);
}
float ribs(vec3 p){
 vec2 a=vec2(abs(p.x)+6.,p.y);float R=length(a),th=atan(a.y,a.x);
 float zc=mod(p.z,5.)-2.5;
 float v=.42+.07*cos(th*170.);                                 // vertebrae
 float r=max(length(vec2(R-22.,zc))-v,-p.y);
 return min(r,max(23.3-R,-p.y));                              // rib arches + vault shell
}
float vcables(vec3 p){
 // cables along the vault, sagging between the ribs
 vec2 a=vec2(abs(p.x)+6.,p.y);float R=length(a),th=atan(a.y,a.x);
 float cs=.017,tc=(floor(th/cs)+.5)*cs,id=floor(th/cs);
 float f=fract(p.z/5.),sag=(.5+.9*h1(id))*sin(3.1416*f);
 float d=length(vec2(R-(21.1-.5*h1(id*1.7)-sag),(th-tc)*R))-.045-.03*h1(id*3.1);
 return max(d*.5,-p.y);
}
float curtain(vec3 p){
 // a curtain of cables falling around the cyborg, and the halo of machines around her face
 vec3 r=p-vec3(0,2.1,40.6);
 float d=41.4-p.z;
 float rr=length(r.xy),an=atan(r.y,r.x);
 for(int i=0;i<5;i++){float R=.42+.35*float(i*i)*.55+.05*BREATH*float(i);
  d=min(d,length(vec2(rr-R,r.z+.1+.12*float(i)))-.035-.012*float(i));}
 float n=44.,ac=(floor(an/(6.2832/n))+.5)*(6.2832/n);
 vec2 dir=vec2(cos(ac),sin(ac));float al=dot(r.xy,dir);
 float cab=length(vec2(length(r.xy-dir*al),r.z+.25+.08*sin(al*2.+ac*3.)))-.028;
 d=min(d,max(cab,.23-al));
 // the waterfall of cables below her: thousands of strands to the floor
 float x=p.x+.13*sin(p.y*1.3+floor(p.x/.09)),cx=(floor(x/.09)+.5)*.09;
 float w=length(vec2(x-cx,(p.z-39.9+.3*sin(cx*2.)-.02*p.y*p.y)))-.028;
 w=max(w,max(p.y-1.75+.3*abs(p.x),abs(p.x)-2.5));
 d=min(d,w*.8);
 return d;
}
float map(vec3 p){
 if(gCharOnly){float d=android(p),m=gM;float x=cyborg(p);if(x<d){d=x;m=gM;gWho=1.;}gM=m;return d;}
 float d=p.y,m=1.;
 float x=ribs(p);if(x<d){d=x;m=8.;}
 x=vcables(p);if(x<d){d=x;m=6.;}
 if(p.z>30.){x=curtain(p);if(x<d){d=x;m=6.;}}
 // the world comes apart into points after the wave has passed
 float ds=U[14].x*(1.-smoothstep(U[13].w-14.,U[13].w-2.,length(p-U[13].xyz)));
 if(ds>0.)d=max(d,dots(p,mix(.9,.1,ds)));
 x=android(p);if(x<d){d=x;m=gM;}
 x=cyborg(p);if(x<d){d=x;m=gM;gWho=1.;}
 gM=m;
 return d;
}
vec3 env(vec3 d){return vec3(.75,.78,.82)*(.3+.4*max(d.y,0.))*LIGHTS+vec3(2.5,2.6,2.8)*smoothstep(.97,.99,dot(d,normalize(vec3(.2,.9,.3))))*LIGHTS;}
vec3 emis(vec3 p,vec3 n,float m){
 vec3 e=vec3(0);
 // cables carry light towards her
 if(m==6.)e+=vec3(.6,.8,1.)*pow(max(sin(p.z*1.3-U[0].x*3.+p.x),0.),30.)*SYS*.3;
 return e;
}
#endif
#ifdef S_SPACE
float map(vec3 p){return android(p);}
vec3 env(vec3 d){return vec3(.5)*(.5+.5*d.y);}
vec3 emis(vec3 p,vec3 n,float m){return vec3(0);}
#endif


// ---------------------------------------------------------------- shading helpers
vec3 nrm(vec3 p,float e){vec2 k=vec2(1,-1)*e;
 return normalize(k.xyy*map(p+k.xyy)+k.yyx*map(p+k.yyx)+k.yxy*map(p+k.yxy)+k.xxx*map(p+k.xxx));}
float softsh(vec3 p,vec3 l,float mx){float s=1.,t=.025;for(int i=0;i<48;i++){float h=map(p+l*t);s=min(s,5.*h/t);t+=clamp(h,.012,.15);if(s<.01||t>mx)break;}return smoothstep(0.,1.,s);}
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
 {vec3 q=vec3(abs(e.x),e.yz);float au=mix(.5,-.5,gEyeSide>0.?gBlink.y:gBlink.x)-KU*q.x*q.x+7.*q.x,ae=atan(e.y,e.z);
  c*=mix(.3,1.,1.-smoothstep(-.18,.02,ae-au));
  c*=mix(.6,1.,smoothstep(-.42+KL*q.x*q.x+7.*q.x,-.25,ae));}
 return c*(1.-F)+spec*F*1.6;
}

// iris colour: green (identity) or heterochromia (left blue, right red)
vec3 irisCol(float side,float mode){
 return mix(vec3(.06,.36,.14),side>0.?vec3(.08,.2,.62):vec3(.62,.05,.04),mode);
}

vec3 shade(vec3 p,vec3 rd,float t,float m){
 float who=gWho;
 vec3 n=nrm(p,max(t*.0004,.00005));
 if(who>.5){hR=cR;hP=cP;gGaze=cGaze;gBlink=U[9].zw;}else{hR=aR;hP=aP;gGaze=aGaze;gBlink=vec2(U[6].w);}
 vec3 L=normalize(vec3(-.3,.9,.35));
 float lt=TESTMODE?1.:LIGHTS;
 if(m==2.){vec3 hp=hR*(p-hP);float side=sign(hp.x+1e-6);gEyeC=EC*vec3(side,1,1);gEyeSide=side;
  return shadeEye(p,rd,n,L,irisCol(side,who>.5?1.:U[7].z),who>.5?U[10].z:U[6].z,who>.5?.25:U[7].w)*(.4+.6*lt);}
 vec3 alb=vec3(.9,.9,.89);float rough=.35,met=0.;
 if(m==3.){alb=vec3(.55,.57,.6);met=1.;rough=.2;}
 if(m==4.)alb=vec3(.78,.66,.66);
 if(m==6.){alb=vec3(.12,.13,.14);rough=.3;}
 if(m==8.){alb=vec3(.93);rough=.2;}
 if(m==9.){alb=vec3(.08,.085,.09);rough=.3;}
 if(m==1.&&p.y>.001&&p.y<3.99){      // wall panel seams
  vec2 w=abs(p.x)>4.9?p.zy:p.xy;vec2 g=abs(fract(w/vec2(1.5,1.))-.5);
  alb*=1.-.3*(1.-smoothstep(.0,.004,.5-max(g.x,g.y)));}
 if(m==1.&&p.y<.001){vec2 g=abs(fract(p.xz)-.5);alb*=1.-.15*(1.-smoothstep(0.,.003,.5-max(g.x,g.y)));}
 if(m<=4.){ // lash line on the upper lid margin, painted brows
  vec3 hp=hR*(p-hP),e=vec3(abs(hp.x),hp.yz)-EC;
  float bx=(abs(hp.x)-.031)/.019;
  if(hp.z>.05&&abs(bx)<1.)alb*=1.-.35*(1.-smoothstep(.0008,.0016,abs(hp.y-.022-.004*(1.-bx*bx)+.002*bx)))*smoothstep(1.,.7,abs(bx));
  float au=mix(.5,-.5,hp.x>0.?gBlink.y:gBlink.x)-KU*e.x*e.x+7.*e.x,ae=atan(e.y,e.z);
  if(length(e)<ER+.0032&&e.z>0.)alb*=mix(1.,.07,1.-smoothstep(.04,.1,abs(ae-au-.03)));
 }
 gCharOnly=p.z>6.2;
#ifdef S_CATH
 gCharOnly=true;
#endif
 float wear=0.;
 if(who>.5&&m==1.){ // older, worn composite: patina, scratches, dried streaks under the eyes, light in the seams
  vec3 hp=hR*(p-hP);
  float pat=fbm(hp*60.);alb*=vec3(.95,.93,.88)*(.82+.25*pat);
  float sc=1.-smoothstep(.0,.03,abs(vn(vec3(hp.x*900.,hp.y*90.+hp.z*400.,1.))-.5));
  alb*=1.-.3*sc*step(.55,vn(hp*40.));
  float st=(1.-smoothstep(.001,.003,abs(abs(hp.x)-.036+.002*sin(hp.y*300.))))*step(hp.y,-.01)*step(-.07,hp.y);
  alb*=1.-.55*st*(1.-smoothstep(-.07,-.01,hp.y)*.6);
  float g=(hp.z-.033-.4*hp.y-2.*hp.y*hp.y)*.9;
  wear=(1.-smoothstep(0.,.0012,abs(g)))*step(0.,hp.z);
 }
 float sh=softsh(p+n*.004,L,3.);gCharOnly=false;
 float oc=ao(p,n,.004+t*.004);
 float wr=max(dot(n,L)+.35,0.)/1.35;
 vec3 h=normalize(L-rd);
 float sp=pow(max(dot(n,h),0.),2./(rough*rough))*(met>0.?2.:.4)*sh;
 float fr=.04+.96*pow(1.-max(dot(n,-rd),0.),5.);
 vec3 amb=vec3(.55,.57,.6)*(.6+.4*n.y)*oc;
 vec3 col=alb*(vec3(1.,.98,.95)*mix(wr,max(dot(n,L),0.)*sh,.7)*1.2+amb)*lt;
 col+=sp*vec3(1)*lt+env(reflect(rd,n))*fr*(met>0.?.9:.15)*oc;
 col+=emis(p,n,m);
 col+=wear*mix(vec3(1.,.1,.05),vec3(.1,.3,1.),step(0.,(hR*(p-hP)).x))*1.5;
#ifdef S_CATH
 if(U[13].w>0.){ // the violet wave: a front of light, then everything it touched keeps glowing
  float dw=length(p-U[13].xyz),fr=exp(-pow((dw-U[13].w)/(.3+.04*U[13].w),2.));
  col+=vec3(.55,.2,1.)*fr*4.+vec3(.35,.2,.7)*(m==6.?1.2:.25)*step(dw,U[13].w)*(.6+.4*sin(dw*3.-U[0].x*6.));
  col+=vec3(1.,.9,1.)*U[14].x*(1.-smoothstep(U[13].w-14.,U[13].w-2.,dw))*2.;
 }
 {vec3 sp=vec3(0,9.,31.),sd=normalize(vec3(0,2.1,40.)-sp),lv=normalize(sp-p);
  float cone=smoothstep(.93,.985,dot(-lv,sd))*U[8].w;
  col+=alb*vec3(.8,.9,1.1)*cone*max(dot(n,lv),0.)*2.;
  col+=alb*vec3(.3,.5,1.)*.4*U[8].w/(1.+dot(p-vec3(0,2.1,40.3),p-vec3(0,2.1,40.3))*.5);}
#endif
 if(U[12].z>-1.){vec3 hp=hR*(p-hP);
  if(length(hp)<.2)col+=vec3(.5,1.,.7)*3.*exp(-pow((hp.y-mix(.1,-.12,U[12].z))/.0012,2.));}
 return col;
}

void main(){
 vec2 uv=(q*2.-1.)*vec2(2.353,1);
 vec3 cp=U[1].xyz,ta=U[2].xyz;
 vec3 fw=normalize(ta-cp),rt=normalize(cross(fw,vec3(0,1,0))),up=cross(rt,fw);
 rt=rt*cos(U[2].w)+up*sin(U[2].w);up=cross(rt,fw);
 vec3 rd=normalize(fw/tan(radians(U[1].w)*.5)+uv.x*rt+uv.y*up);
 aP=hP=U[4].xyz;
 {float cy=cos(U[5].x),sy=sin(U[5].x),cx=cos(U[5].y),sx=sin(U[5].y);
  aR=hR=transpose(mat3(cy,0,-sy,0,1,0,sy,0,cy)*mat3(1,0,0,0,cx,-sx,0,sx,cx));}
 aGaze=normalize(vec3(sin(U[6].x),sin(U[6].y),1));
 cP=U[8].xyz;
 {float cy=cos(U[9].x),sy=sin(U[9].x),cx=cos(U[9].y),sx=sin(U[9].y);
  cR=transpose(mat3(cy,0,-sy,0,1,0,sy,0,cy)*mat3(1,0,0,0,cx,-sx,0,sx,cx));}
 cGaze=normalize(vec3(sin(U[10].x),sin(U[10].y),1));
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
#ifdef S_LAB
 {// the glass partition in front of the racks
  float tg=(-3.25-cp.x)/rd.x;vec3 g=cp+rd*tg;
  if(tg>0.&&tg<t&&abs(g.z+.5)<5.&&g.y<4.&&!TESTMODE){
   float F=.04+.96*pow(1.-abs(rd.x),5.);
   col=col*vec3(.86,.93,.9)+env(reflect(rd,vec3(1,0,0)))*F*.6;
  }
 }
#endif
 if(any(isnan(col)))col=vec3(0);
 o=vec4(col*exp2(U[3].z),1);
}
