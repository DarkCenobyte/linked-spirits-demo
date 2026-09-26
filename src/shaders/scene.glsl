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
  e+=vec3(.1,.8,.3)*step(.55,h2(floor(u)+floor(U[0].x*2.)))*step(.3,fract(u.y))*step(3.5,SYS)*step(.1,LIGHTS);}
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
// three soft cables fall from the mass along the sides of her face (never over the eyes or cheeks)
vec3 bez(vec3 a,vec3 b,vec3 c,vec3 d,float s){float t=1.-s;return a*t*t*t+3.*b*t*t*s+3.*c*t*s*s+d*s*s*s;}
float cybody(vec3 p){
 vec3 h=cR*(p-cP);
 if(dot(h,h)>.06)return length(h)-.2;
 float d=1e5;
 for(int i=0;i<3;i++){float sw=.004*sin(U[0].x*.9+float(i)*2.);
  vec3 a=i==0?vec3(-.03,.115,-.01):i==1?vec3(-.01,.125,-.03):vec3(.025,.115,-.01),
       b=i==0?vec3(-.135,.07,.05):i==1?vec3(-.16,.09,.035):vec3(.13,.06,.055),
       c=i==0?vec3(-.075,-.07,.04):i==1?vec3(-.1,-.09,.03):vec3(.08,-.08,.035),
       e=i==0?vec3(-.035,-.15,-.02):i==1?vec3(-.06,-.17,-.03):vec3(.03,-.16,-.02);
  b.x+=sw;c.x+=sw*1.5;
  b.z+=.05;c.z+=.05;
  vec3 o=a;float r=i==1?.0055:.0065;
  for(int k=1;k<=8;k++){vec3 n=bez(a,b,c,e,float(k)/8.);d=min(d,cap(h,o,n,r));o=n;}
 }
 gM=8.;return d;
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
// the far wall: her head emerges at the apex of a gigantic dome of woven cables, strapped by
// metal hoops and bolted to the wall; loops of cable hang around it, strands dangle from the vault
const vec4 HL[9]=vec4[](vec4(-.95,1.25,.5,.35),vec4(.85,1.35,.45,.3),vec4(-.45,.65,.6,.4),vec4(.55,.55,.5,.45),
 vec4(-1.7,2.25,.5,.3),vec4(1.55,2.5,.6,.35),vec4(.05,3.,.7,.3),vec4(1.95,1.05,.5,.4),vec4(-2.2,.95,.6,.45));
float zdome(float r){return 39.99+.11*smoothstep(.15,.6,r)+1.3*pow(min(r/3.6,1.),2.);}   // it swallows the back of her head
float curtain(vec3 p,out float m){
 m=6.;
 vec2 v=(p.xy-vec2(0,2.1))*vec2(.85,1);float r=length(v),an=atan(v.y,v.x),zf=zdome(r)+.03*sin(an*5.+r*3.)*min(r,1.);
 float d=min(41.4-p.z,(zf+.05-p.z)*.75);
 for(int i=0;i<2;i++){                                   // two layers of cables woven over each other, converging on her
  float fi=float(i),n=fi<.5?70.:43.,sec=6.2832/n,a=an+.12*sin(r*1.3+fi*2.)+.05*sin(r*3.7+fi);
  float id=floor(a/sec),w=a-(id+.5)*sec+.008*sin(r*5.+id*1.7);
  float rc=min(fi<.5?.035:.055,r*sec*.36)*(.75+.5*h1(id+fi*50.));
  float zc=zf+rc*(1.+.7*sin(r*5.+id*2.1+fi*3.14));
  d=min(d,(length(vec2(w*r,p.z-zc))-rc)*.8);
 }
 float hm=length(vec2(r-.21,p.z-zdome(.21)+.022))-.011;  // a thin halo ring around her head
 for(int i=0;i<3;i++){float R=1.1+float(i)*.9;hm=min(hm,length(vec2(r-R,p.z-zdome(R)+.01))-.03-.01*float(i));}  // hoops
 float ba=mod(an+.3,.7854)-.3927;
 hm=min(hm,box(vec3(ba*r,r-3.25,p.z-41.),vec3(.12,.3,.4),.02));     // brackets bolted into the wall
 if(hm<d){d=hm;m=3.;}
 if(p.z<39.3)return min(d,39.5-p.z);
 float hc=1e5;
 for(int i=0;i<9;i++){vec4 L=HL[i];float dx=p.x-L.x,u=dx/L.z;          // loops sagging in front of the mass
  float f=L.y+L.w*u*u,fd=2.*L.w*u/L.z,ze=zdome(length(vec2(L.z*.85,L.y+L.w-2.1)))-.03;
  hc=min(hc,max(length(vec2((p.y-f)/sqrt(1.+fd*fd),p.z-ze+.18*(1.-u*u)))-.016-.008*h1(float(i)),abs(dx)-L.z));}
 for(int i=0;i<7;i++){float fi=float(i),x0=(fi-3.)*.9+.3*h1(fi),z0=39.6+.8*h1(fi*2.3),yb=2.8+2.*h1(fi*4.1);
  float k=max(0.,(12.-p.y)/(12.-yb));                      // strands dangling from the vault, swaying
  vec2 o=vec2(x0+.25*k*k*sin(U[0].x*.4+fi*2.),z0+.2*k*k*cos(U[0].x*.33+fi));
  hc=min(hc,max(length(p.xz-o)-.018,yb-p.y)*.9);}
 if(hc<d){d=hc;m=6.;}
 return d;
}
float map(vec3 p){
 float d=1e5,m=1.,w=0.,x;
 if(!gCharOnly){
  d=p.y;
  x=ribs(p);if(x<d){d=x;m=8.;}
  x=vcables(p);if(x<d){d=x;m=6.;}
  if(p.z>30.){float cm;x=curtain(p,cm);if(x<d){d=x;m=cm;}}
 }
 x=cybody(p);if(x<d){d=x;m=gM;w=1.;}
 // the world, and the cables that held her, come apart into points after the wave has passed
 float ds=U[14].x*(1.-smoothstep(U[13].w-14.,U[13].w-2.,length(p-U[13].xyz)));
 if(ds>0.)d=max(d,dots(p,mix(.9,.1,ds)));
 x=android(p);if(x<d){d=x;m=gM;w=0.;}
 x=cyborg(p);if(x<d){d=x;m=gM;w=1.;}
 gM=m;gWho=w;
 return d;
}
vec3 env(vec3 d){return vec3(.75,.78,.82)*(.3+.4*max(d.y,0.))*LIGHTS+vec3(2.5,2.6,2.8)*smoothstep(.97,.99,dot(d,normalize(vec3(.2,.9,.3))))*LIGHTS;}
vec3 emis(vec3 p,vec3 n,float m){
 vec3 e=vec3(0);
 // cables carry light towards her
 if(m==6.)e+=vec3(.6,.8,1.)*pow(max(sin(p.z>39.?length(p.xy-vec2(0,2.1))*4.+U[0].x*3.:p.z*1.3-U[0].x*3.+p.x),0.),30.)*SYS*.3;
 return e;
}
#endif
#ifdef S_SPACE
// ---------------------------------------------------------------- cyberspace: faces as masks, bodies of light, worlds
// U[16] planet centre+radius, U[17] type, ring inner, ring outer, ring tilt, U[18] sun dir + star size,
// U[19] speed, nebula, iris-cosmos, third silhouette; U[15] second planet centre+radius, U[14].x its type
float mask(vec3 p,mat3 R,vec3 P,vec2 bl,float mo,float mr,float sty,vec3 gz){
 vec3 h=R*(p-P);
 if(length(h)>.17)return length(h)-.14;
 gGaze=gz;
 return max(face(h,bl,mo,mr,sty),.028-h.z-.25*dot(h.xy,h.xy));
}
float map(vec3 p){
 float d=mask(p,aR,aP,vec2(U[6].w),U[7].x,U[7].y,0.,aGaze),m=gM;gWho=0.;
 float x=mask(p,cR,cP,U[9].zw,U[10].w,U[11].x,1.,cGaze);
 if(x<d){d=x;m=gM;gWho=1.;}
 gM=m;
 return d;
}
vec3 env(vec3 d){return vec3(.08,.07,.14)+vec3(1.,.95,.9)*.8*smoothstep(.9,1.,dot(d,normalize(U[18].xyz)));}
vec3 emis(vec3 p,vec3 n,float m){return vec3(0);}
vec3 stars(vec3 d){
 vec3 c=vec3(0);
 for(int i=0;i<3;i++){float sc=180.+float(i)*170.;vec3 q=d*sc,id=floor(q),f=fract(q)-.5;
  vec3 r=h3(dot(id,vec3(1,57,113))+float(i)*7.);
  float s=smoothstep(.12,0.,length(f-(r-.5)*.7))*step(.82,r.x);
  c+=mix(vec3(.6,.7,1.),vec3(1.,.8,.7),r.y)*s*(.5+2.*r.z*r.z);}
 return c;
}
vec3 nebula(vec3 d){
 float n=fbm(d*2.5+vec3(0,0,U[0].x*.01)),m=fbm(d*5.+n*2.);
 vec3 c=mix(vec3(.35,.05,.1),vec3(.05,.12,.45),smoothstep(.3,.7,m))*pow(n,2.5)*1.6;
 c+=vec3(.3,.2,.5)*pow(max(0.,1.-abs(d.y*3.+.5*sin(d.x*4.))),6.)*.25;   // an interstellar current
 return c*U[19].y;
}
vec3 surf(vec3 n,float ty,vec3 sun,vec3 rd){
 float tt=U[0].x*.01;
 vec3 q=vec3(n.x*cos(tt)-n.z*sin(tt),n.y,n.x*sin(tt)+n.z*cos(tt));
 float f=fbm(q*2.6),g=fbm(q*9.+f),dif=max(dot(n,sun),0.),term=smoothstep(-.05,.25,dot(n,sun));
 vec3 c;float spec=0.,cl=0.;
 if(ty<.5){                                           // an earth that is not the Earth
  float land=smoothstep(.5,.53,f+.08*g);
  c=mix(vec3(.02,.07,.2),mix(vec3(.12,.25,.08),vec3(.45,.36,.22),smoothstep(.55,.7,f+.2*g)),land);
  c=mix(c,vec3(.9),smoothstep(.75,.85,abs(n.y)+.1*g));
  spec=(1.-land)*.6;cl=smoothstep(.55,.75,fbm(q*5.+vec3(tt*3.,0,0)));
 }else if(ty<1.5){                                    // desert
  float r=1.-abs(g*2.-1.);
  c=mix(vec3(.5,.28,.12),vec3(.85,.62,.38),f)*(.7+.4*r);c*=1.-.5*smoothstep(.62,.66,fbm(q*5.));
 }else if(ty<2.5){                                    // ocean world
  float isl=smoothstep(.66,.68,f+.1*g);
  c=mix(vec3(.01,.06,.2),vec3(.3,.35,.2),isl);spec=(1.-isl);
  vec3 w=q*3.+.8*vec3(fbm(q*4.),fbm(q*4.+3.),fbm(q*4.+7.));float sw=fbm(w*2.);
  cl=smoothstep(.55,.85,sw)*.7;
 }else{                                               // gas giant
  float b=sin(q.y*18.+f*5.+g*2.);
  c=mix(vec3(.72,.55,.38),vec3(.95,.88,.78),.5+.5*b);c=mix(c,vec3(.55,.3,.2),smoothstep(.6,.9,sin(q.y*7.+f*3.)));
  c*=.8+.3*g;
 }
 c=mix(c,vec3(1),cl);
 vec3 col=c*(dif*1.2+.02)*term;
 col+=spec*(1.-cl)*pow(max(dot(reflect(rd,n),sun),0.),60.)*term*vec3(1.,.9,.7);
 return col;
}
vec3 world(vec3 ro,vec3 rd,vec4 pl,float ty,float ri,float rO,float tilt,inout float tMin){
 vec3 sun=normalize(U[18].xyz),col=vec3(0),oc=ro-pl.xyz;
 float b=dot(oc,rd),c=dot(oc,oc)-pl.w*pl.w,h=b*b-c,tp=1e20;
 vec3 atm=ty<.5?vec3(.35,.6,1.):ty<1.5?vec3(1.,.6,.35):ty<2.5?vec3(.3,.55,1.):vec3(.9,.75,.6);
 if(h>0.){tp=-b-sqrt(h);
  if(tp>0.){vec3 n=normalize(oc+rd*tp);col=surf(n,ty,sun,rd);
   float fr=pow(1.-max(dot(n,-rd),0.),3.);col+=atm*fr*max(dot(n,sun)+.3,0.)*.9;tMin=min(tMin,tp);}
 }
 // atmosphere halo around the limb
 float dm=length(oc-rd*b)-pl.w;
 if(b<0.)col+=atm*exp(-max(dm,0.)/(pl.w*.04))*.5*max(dot(normalize(oc-rd*b),sun)+.4,0.)*step(tp,1e19)*0.+atm*exp(-max(dm,0.)/(pl.w*.03))*.35*step(0.,dm);
 // rings
 if(rO>0.){
  vec3 rn=normalize(vec3(sin(tilt),cos(tilt),.25));
  float tr=dot(pl.xyz-ro,rn)/dot(rd,rn);
  if(tr>0.){vec3 rp=ro+rd*tr-pl.xyz;float r=length(rp);
   if(r>ri&&r<rO){
    float u=(r-ri)/(rO-ri);
    float den=(.55+.45*sin(u*90.))*(.6+.4*sin(u*23.+1.))*smoothstep(0.,.03,u)*(1.-smoothstep(.97,1.,u))*(.7+.3*vn2(vec2(u*400.,1.)));
    den*=1.-.85*smoothstep(.44,.46,u)*(1.-smoothstep(.49,.51,u));      // a division
    // shadow of the planet on the ring
    vec3 w=rp;float bb=dot(w,sun),sh=step(0.,bb)+step(pl.w*pl.w,dot(w,w)-bb*bb)>0.?1.:0.;
    vec3 rc=vec3(.95,.88,.78)*den*(.15+.9*sh)*(.4+.6*abs(dot(rn,sun)));
    float a=den*.85;
    if(tr<tp)col=mix(col,rc,a);else col+=rc*a*(tp>1e19?1.:0.);
    tMin=min(tMin,tr);
   }
  }
 }
 return col;
}
vec3 space(vec3 ro,vec3 rd){
 vec3 col=stars(rd)*(1.-U[19].x*.5)+nebula(rd);
 vec3 sun=normalize(U[18].xyz);
 // the great star (oneiric): granulated disc and corona
 if(U[18].w>0.){float cs=dot(rd,sun),rs=U[18].w;float a=acos(clamp(cs,-1.,1.));
  vec3 gr=vec3(1.,.72,.4)*(.8+.4*fbm(rd*60.+U[0].x*.05));
  col+=a<rs?gr*6.*mix(.55,1.,sqrt(max(0.,1.-a*a/(rs*rs)))):vec3(1.,.6,.3)*2.*exp(-(a-rs)/(rs*.25))+vec3(1.,.5,.3)*.6*exp(-(a-rs)/(rs*1.5));
 }else col+=vec3(1.,.9,.8)*6.*smoothstep(.9995,.9999,dot(rd,sun))+vec3(1.,.8,.6)*.1*pow(max(dot(rd,sun),0.),40.);
 float tm=1e20;
 if(U[15].w>0.){vec3 w2=world(ro,rd,U[15],U[14].x,0.,0.,0.,tm);if(tm<1e19)col=w2;else col+=w2;}
 float tm2=1e20;
 if(U[16].w>0.&&U[19].z<=0.){vec3 w=world(ro,rd,U[16],U[17].x,U[17].y,U[17].z,U[17].w,tm2);if(tm2<1e19&&tm2<tm)col=w+(tm2<1e19?vec3(0):col);else col+=w;}
 // the iris becomes a planetary system: a disc of stroma the size of worlds. U[19].z-1 morphs it:
 // the pupil ignites into a star, the fibres settle into dust lanes, worlds appear on their orbits
 if(U[19].z>0.){
  float t=(U[16].z-ro.z)/rd.z;vec3 p=ro+rd*t;
  if(t>0.){vec2 d=(p.xy-U[16].xy)/U[16].w;float r=length(d),a=atan(d.y,d.x),mf=clamp(U[19].z-1.,0.,1.),isP;
   vec3 ir=iris(r,a,.3,mix(vec3(.06,.36,.14),vec3(.1,.25,.7),smoothstep(0.,.5,mf)),isP)*2.5;
   ir=mix(ir,ir.bgr*vec3(1.,.4,.6),smoothstep(.4,.9,fbm(p/200.))*smoothstep(.1,.6,mf))+vec3(.3,.1,.5)*pow(fbm(p/90.),3.);
   float lane=pow(.5+.5*sin(r*70.+fbm(vec3(d*6.,1.))*5.),3.);
   ir=mix(ir,(ir*.3+vec3(.5,.45,.7))*lane*(1.-smoothstep(.95,1.,r)),mf*.75);
   float ring=exp(-pow((r-1.02)/.015,2.))*2.;
   vec3 c=r<1.?(isP>.5?vec3(1.,.7,.4)*exp(-abs(r-.3)*80.)*4.:ir):vec3(.95,.85,.7)*ring*(.5+fbm(vec3(a*40.,r*9.,0))*mf);
   c+=vec3(1.,.8,.55)*mf*(exp(-r*r*120.)*30.+exp(-r*9.)*1.5);             // the star
   for(int i=0;i<5;i++){                                                  // orbits
    float ro2=.42+float(i)*.14;
    c+=vec3(.6,.6,.8)*.3*mf*exp(-pow((r-ro2)/.004,2.));
   }
   col=mix(col,c+col*step(1.05,r),U[19].z>1.05||r<1.08?1.:0.);
  }
  float mf=clamp(U[19].z-1.,0.,1.);
  for(int i=0;i<5;i++){                                                   // worlds on their orbits, lit by the star
   float fi=float(i),th=fi*2.4+U[0].x*.12/(1.+fi),rad=U[16].w*(.04+.03*h1(fi*7.1));
   vec3 pc=U[16].xyz+U[16].w*(.42+fi*.14)*vec3(cos(th),sin(th),0),oc=ro-pc;
   float b=dot(oc,rd),h=b*b-dot(oc,oc)+rad*rad;
   if(h>0.){vec3 hp=ro+rd*(-b-sqrt(h)),n=(hp-pc)/rad;
    vec3 pcol=mix(mix(vec3(.25,.45,.8),vec3(.85,.6,.35),h1(fi*3.1)),vec3(.8,.75,.65),step(.6,h1(fi*5.3)));
    pcol*=.75+.5*fbm(n*vec3(3,3,12)+fi);
    col=mix(col,pcol*(max(dot(n,normalize(U[16].xyz-hp)),0.)*1.8+.02)+vec3(.3,.4,.8)*pow(1.-abs(dot(n,rd)),4.)*.4,mf);}
  }
 }
 return col;
}
#endif


// ---------------------------------------------------------------- shading helpers
vec3 nrm(vec3 p,float e){vec2 k=vec2(1,-1)*e;
 return normalize(k.xyy*map(p+k.xyy)+k.yyx*map(p+k.yyx)+k.yxy*map(p+k.yxy)+k.xxx*map(p+k.xxx));}
float softsh(vec3 p,vec3 l,float mx){float s=1.,t=.02+.02*h2(gl_FragCoord.xy+fract(U[0].x)*9.);for(int i=0;i<48;i++){float h=map(p+l*t);s=min(s,5.*h/t);t+=clamp(h,.012,.15);if(s<.01||t>mx)break;}return smoothstep(0.,1.,s);}
float ao(vec3 p,vec3 n,float sc){float a=0.,w=1.;for(int i=1;i<6;i++){float h=sc*float(i);a+=w*(h-map(p+n*h));w*=.6;}return clamp(1.-a/(sc*3.),0.,1.);}

vec3 shadeEye(vec3 p,vec3 rd,vec3 n,vec3 L,vec3 col,float pupil,float glow){
 vec3 hp=hR*(p-hP),hd=hR*rd,hn=hR*n,hl=hR*L;
 vec3 e=hp-gEyeC;
 float onC=dot(normalize(e),gGaze);
 vec3 c;
 float F=.03+.97*pow(1.-max(dot(-hd,hn),0.),5.);
 vec3 spec=env(transpose(hR)*reflect(hd,hn));
 spec+=(gEyeSide>0.?vec3(.2,.4,1.):vec3(1.,.15,.1))*U[62].w*8.*smoothstep(.9975,.9992,dot(normalize(transpose(hR)*reflect(hd,hn)),normalize(vec3(.35*gEyeSide,.55,.75))));
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
 if(m==10.){alb=vec3(.52,.51,.49);rough=.25;}
 if(m==9.){alb=vec3(.08,.085,.09);rough=.3;if(length(hR*(p-hP))<.12)alb=vec3(.035,.03,.032);}   // mouth interior
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
  wear=(1.-smoothstep(0.,.0012,abs(g)))*step(0.,hp.z)*step(-.1,hp.y);
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
#ifdef S_LAB
 if(U[7].w>0.&&!TESTMODE){   // her eyes light the dark room: blue (left), red (right)
  for(int k=0;k<2;k++){vec3 e=(k==0?U[13]:U[14]).xyz,l=e-p;float dl=dot(l,l);
   col+=alb*(k==0?vec3(.15,.35,1.):vec3(1.,.12,.08))*U[7].w*.5*max(dot(n,normalize(l)),0.)/(1.+dl*12.);}
 }
#endif
 col+=wear*mix(vec3(1.,.1,.05),vec3(.1,.3,1.),step(0.,(hR*(p-hP)).x))*1.5;
#ifdef S_SPACE
 {vec3 hp=hR*(p-hP);float e=.028-hp.z-.25*dot(hp.xy,hp.xy);
  col+=(who>.5?vec3(1.,.2,.1):vec3(.2,.45,1.))*3.*exp(-abs(e)*900.);}
#endif
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
#ifdef S_SPACE
 col=space(cp,rd);
#endif
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
