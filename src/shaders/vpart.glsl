// ---- particles: everything is generated from gl_VertexID ------------------------
// U[63]: x mode, y first particle, z intensity, w end.  U[16..19]: mode parameters.
out vec4 pc;          // colour (premultiplied intensity)
out float pdist;      // distance from the camera (for depth occlusion)
out float psz;        // sprite size in pixels
out float pline;      // 1 for streak lines
layout(binding=4) uniform sampler2D SC;   // the frozen frame: colour
layout(binding=5) uniform sampler2D SD;   // and distance along each ray
uint hu(uint x){x^=x>>16;x*=0x7feb352dU;x^=x>>15;x*=0x846ca68bU;return x^x>>16;}     // integer hash: sin() hashes break down past ~1e5
float ph(float n){return float(hu(floatBitsToUint(n)))*2.3283e-10;}
vec3 ph3(float n){uint h=hu(floatBitsToUint(n));return vec3(h,hu(h),hu(h^0x9e3779b9U))*2.3283e-10;}
float n1(float x){float f=fract(x);return mix(ph(floor(x)),ph(floor(x)+1.),f*f*(3.-2.*f));}
vec3 sdir(float n){vec3 r=ph3(n);float z=r.x*2.-1.,a=r.y*6.2831;return vec3(sqrt(1.-z*z)*vec2(cos(a),sin(a)),z);}
// gentle curl-like drift
vec3 drift(vec3 p,float t){return vec3(sin(p.y*1.3+t*.3+p.z),sin(p.z*1.1+t*.27+p.x*.7),sin(p.x*1.2+t*.23+p.y*.5));}
// the body surface. torso: height y along the spine, angle th; limbs: capsules between joints
vec3 torsoAt(vec3 pel,vec3 sd,vec3 up,vec3 fw,float y,float th,float k){
 float rx=.105+.055*exp(-pow(y/.1,2.))+.04*exp(-pow((y-.35)/.08,2.))+.03*exp(-pow((y-.44)/.03,2.));
 float rz=.075+.025*exp(-pow((y-.33)/.1,2.));
 float bust=.035*exp(-pow((y-.31)/.045,2.))*exp(-pow((abs(cos(th))*rx-.058)/.035,2.))*step(0.,sin(th));
 vec3 l=vec3(cos(th)*rx,y,sin(th)*(rz+bust))*k;
 return pel+sd*l.x+up*l.y+fw*l.z;
}
vec3 limbAt(vec3 a,vec3 b,float ra,float rb,float u,vec3 dd){
 vec3 dir=normalize(b-a+vec3(1e-5,0,0));return mix(a,b,u)+normalize(cross(dir,dd)+1e-5)*mix(ra,rb,u);}
// a point of a body (joints at U[B..B+19]); w: -1 hidden, 0 torso, 2 limbs, 3 armour of light.
// The armour is one piece on the body itself: a yoke over the chest and back, the gorget rising from
// it up the neck, the pauldrons flowing from it over the shoulders. Lamellar gaps are left dark.
vec4 bodyPt(int B,float n){
 vec3 r=ph3(n*.71),dd=sdir(n*3.3);float s=ph(n*1.31),k=B>30?1.12:1.;      // the red body is larger in every dimension
 vec3 pel=U[B].xyz,chs=U[B+1].xyz,up=normalize(chs-pel+1e-5),sd=normalize(U[B+4].xyz-U[B+8].xyz+1e-5),fw=normalize(cross(sd,up));
 sd=cross(up,fw);
 vec3 nk=U[B+2].xyz,hd=U[B+3].xyz,nt=hd-up*.06;
 float th=r.y*6.2832,yb=.43-.05*pow(abs(sin(th)),2.);                      // the armour's lower edge: a bib, front and back
 if(s<.36){float y=mix(-.1,.47,r.x);                                         // torso (its top belongs to the armour)
  return vec4(torsoAt(pel,sd,up,fw,y,th,k),y>yb?-1.:0.);}
 if(s<.42){vec3 e=dd*vec3(.068,.088,.086);                                   // head: the face is the mask
  return vec4(hd+(sd*e.x+up*(e.y+.02)+fw*(e.z-.02))*mix(1.,k,.3),dot(dd,fw)>-.1?-1.:1.);}
 if(s<.58){                                                                   // the armour
  float z=r.z,u=r.x;vec3 P;float gap;
  if(z<.4){float y=mix(yb,.47,u);P=torsoAt(pel,sd,up,fw,y,th,k*1.02);gap=fract((y-yb)/.022);}       // yoke
  else if(z<.6){P=limbAt(nk-up*.01,nt,.041*k,.036*k,u*.8,dd);gap=fract(u*5.);}                     // gorget
  else{int sdn=z<.8?4:8;vec3 S=U[B+sdn].xyz;                                                         // pauldrons
   if(fract(z*5.)<.5){vec3 ax=normalize(normalize(S-(pel+up*.43*k))*.7+up),d=dd-2.*min(dot(dd,ax),0.)*ax;
    P=S+d*.047*k;gap=fract(dot(d,ax)*3.5);}                                                          // a rounded cap
   else{float v=u*.3;P=limbAt(S,U[B+sdn+1].xyz,.047*k,.042*k,v,dd);gap=fract(v*14.);}}
  return vec4(P,gap<.16?-1.:3.);
 }
 vec3 a,b;float ra,rb,u=r.x;
 if(s<.6){a=nk;b=nt;ra=.037;rb=.031;u=mix(.8,1.,u);}                         // what shows of the neck above the gorget
 else if(s<.78){int side=s<.69?0:4,m=int(ph(n*5.1)*2.99);a=U[B+4+side+m].xyz;b=U[B+5+side+m].xyz;ra=m==0?.041:m==1?.03:.02;rb=m==0?.032:m==1?.022:.012;
  if(m==0)u=mix(.3,1.,u);}                                                    // the shoulder belongs to the pauldron
 else{int side=s<.89?0:4,m=int(ph(n*5.3)*2.99);a=U[B+12+side+m].xyz;b=U[B+13+side+m].xyz;ra=m==0?.074:m==1?.045:.03;rb=m==0?.047:m==1?.028:.02;}
 return vec4(limbAt(a,b,ra*k,rb*k,u,dd),2);
}
void main(){
 float i=float(gl_VertexID),t=U[0].x,mode=U[63].x;
 vec3 p=vec3(0),col=vec3(1);float sz=.004,inten=U[63].z,en=0.;
 vec3 r=ph3(i*.137);
 if(mode<1.5){                                   // 1: motes of light floating in the room
  vec3 c=U[16].xyz;vec3 ext=vec3(U[16].w,U[16].w*.45,U[16].w);
  p=c+(r-.5)*2.*ext;if(U[17].y>.5)p=c+sdir(i*.77)*U[16].w*pow(r.x,.6);p+=drift(p,t)*.15;
  vec3 h=U[4].xyz-p;float dh=length(h);
  p+=h/dh*U[17].x*.3*exp(-dh*1.5)*sin(t*.7+i);        // they begin to react to her
  if(U[17].y<.5)p=c+mod(p-c+ext,2.*ext)-ext;
  col=mix(mix(vec3(.9,1.,.95),vec3(.3,1.,.5),step(.8,r.x)),mix(vec3(.2,.4,1.),vec3(1.,.15,.1),step(.5,r.z)),U[17].y)*(.5+.5*sin(t*(1.+r.y*2.)+i))
     *smoothstep(.6,1.8,length(p-U[1].xyz));     // discreet: none drifts right in front of the lens
  sz=.004;
 }else if(mode<2.5){                             // 2: the first green lights in the dark
  float k=mod(i,4.);p=vec3((mod(k,2.)*2.-1.)*(k<2.?1.7:2.5),k<2.?1.45:2.1,k<2.?-1.55:-2.2);
  col=vec3(.2,1.,.45)*step(k+.5,U[12].y)*4.;sz=.02;
 }else if(mode<4.5){                             // 3: cyberspace (4: inside a ring)
  if(i<60000.){                                      // bodies of light: blue android, red cyborg, and sometimes a third
   float who=step(.45,ph(i*.91));
   vec4 b=bodyPt(who>.5?40:20,i);
   p=b.xyz+drift(b.xyz*6.,t*1.5)*.01;
   inten*=step(-.5,b.w);
   col=b.w>2.5?(who>.5?vec3(.04,.22,1.)*1.7:vec3(.06,1.,.22)*1.3):who>.5?vec3(1.,.13,.08):vec3(.15,.4,1.);
   float th=U[19].w*step(ph(i*.37),.55);          // the climax: they interpenetrate
   if(th>0.){vec4 b2=bodyPt(who>.5?20:40,i);vec3 m=(b.xyz+b2.xyz)*.5;p=mix(p,m+drift(m*9.,t*3.)*.02,th);
    col=mix(col,vec3(1.,.95,1.)*(.8+.4*sin(t*20.+i)),th*.8)+vec3(1,0,0)*th*step(.8,ph(i*1.1))*sin(t*30.+i)+vec3(0,0,1)*th*step(.8,ph(i*1.3))*sin(t*27.+i);}
   col*=(b.w>2.5?.9+.2*r.z:.5+.9*r.z)*(who>.5?U[59].w:1.);sz=b.w>2.5?.0036:.0032;
  }else if(i<120000.){                               // the field: dust of stars travelling past
   vec3 c=U[1].xyz;float E=40.;
   p=(r-.5)*2.*E;p.z+=t*U[19].x*25.;p=c+mod(p-c+E,2.*E)-E;
   col=mix(vec3(.6,.7,1.),vec3(1.,.7,.6),ph(i*.3))*(.3+ph(i*.7));sz=.03;
   if(mode>3.5){                                  // 4: close up, the ring is a crowd of ice and rock under their feet
    E=12.;vec3 C=U[16].xyz;c.y=C.y;p=(r-.5)*2.*E;p.z+=C.z;p=c+mod(p-c+E,2.*E)-E;
    float u=(length(p.xz-C.xz)-U[17].y)/(U[17].z-U[17].y),g=0.,
          den=(.3+.7*vn2(vec2(u*40.,.5)))*(.5+.5*vn2(vec2(u*230.,7.5)))*(1.-.85*smoothstep(.44,.46,u)*(1.-smoothstep(.49,.51,u)));   // the ring's own bands
    p.y=c.y+pow(r.y,3.)*.2-.03;
    for(int f=0;f<4;f++){vec3 ft=U[35+f%2*4+f/2*20].xyz,q=p-ft;g+=exp(-dot(q.xz,q.xz)*18.)*step(ft.y,c.y+.12);}   // the four toes
    p.y+=g*.1*ph(i*.5);                            // a footfall stirs the chunks
    float ice=step(.85,ph(i*.9));
    col=mix(vec3(.95,.86,.72),vec3(.72,.76,.85),vn2(vec2(u*13.,3.5)))*(.2+.45*ph(i*.3)+ice*1.2+g*2.)*step(ph(i*.7),den)*smoothstep(E,E*.6,length(p.xz-c.xz));
    sz=.008+.035*pow(ph(i*.4),3.);}
  }else if(i<136000.){                           // they shed light as they fly: sparks leave each body and fall behind
   float j=i-120000.,w=step(8000.,j),a=fract(ph(j*.37)+t*(.4+.3*ph(j*.9)));
   vec4 b=bodyPt(w>.5?40:20,j*1.7);
   p=b.xyz+vec3(0,0,a*(.3+5.*U[19].x))+(ph3(j*.9)-.5)*a*.5;
   col=(w>.5?vec3(1.,.3,.2):vec3(.3,.55,1.))*(1.-a)*(1.-a)*(.5+ph(j*.2))*U[67].x*step(-.5,b.w);sz=.005;
  }else if(i<150000.){                           // motes of the dream: tiny lights drifting around them
   float j=i-136000.;vec3 h=ph3(j*.61);
   p=(h-.5)*vec3(16,7,16)+vec3(0,1.3,0);p+=drift(p*.4,t*.6)*.8;
   col=mix(mix(vec3(.3,.6,1.),vec3(1.,.35,.5),step(.5,h.x)),vec3(.5,1.,.8),step(.8,h.z))*(.4+.6*pow(.5+.5*sin(t*(1.+h.y*3.)+j),4.))*U[67].y;
   sz=.004+.006*h.y;
  }else{                                         // the great disc: a spiral galaxy, a disc where worlds are born, a ring
   float j=i-150000.,kd=U[69].z,R=U[68].w,ri=0.,tl=U[69].x;vec3 h=ph3(j*.113),C=U[68].xyz,nn;
   float rr=pow(h.x,.6),th=h.y*6.2832,den=1.;
   if(kd>1.5){C=U[16].xyz;R=U[17].z;ri=U[17].y/R;rr=mix(ri,1.,h.x);
    float u=(rr-ri)/(1.-ri);den=(.3+.7*n1(u*40.))*(.5+.5*n1(u*230.))*(1.-.85*smoothstep(.44,.46,u)*(1.-smoothstep(.49,.51,u)));}
   nn=normalize(vec3(sin(tl),cos(tl),U[67].z));
   float arm=step(h.z,.55);                                                         // a galaxy: two arms, and the old disc between them
   if(kd<.5){rr=arm>.5?pow(h.x,.7):-log(1.-h.x*.95)*.33;th=arm>.5?floor(h.y*2.)*3.1416+log(rr*12.+1.)*2.6+(ph(j*.3)+ph(j*.8)-1.)*(.3+.35*rr):th;}
   if(kd>.5&&kd<1.5){rr=mix(.03,1.,h.x*h.x);den=1.-.9*exp(-pow(sin(rr*31.)*3.,2.))*step(.15,rr);}                // gaps opened by young worlds
   th-=t*U[69].y/(.2+rr);                          // inner orbits turn faster
   float r0=rr*R;vec3 lp=vec3(cos(th)*r0,(ph(j*.71)-.5)*R*.012*(1.2-rr),sin(th)*r0);
   if(kd<.5&&ph(j*.4)<.07){lp=sdir(j*1.3)*R*.1*pow(ph(j*.5),1.5);lp.y*=.6;rr=0.;}   // the bulge
   if(kd>.5&&kd<1.5&&h.x<.03){lp=sdir(j)*R*.004;rr=.01;den=8.;}                  // the young star at the centre
   vec3 e1=normalize(cross(nn,vec3(0,0,1))+vec3(1e-4,0,0)),e2=cross(e1,nn);
   p=C+e1*lp.x+nn*lp.y+e2*lp.z;
   col=kd<.5?(arm>.5?mix(vec3(1.,.85,.6),vec3(.4,.6,1.),smoothstep(.02,.3,rr))*.9:vec3(1.,.82,.6)*.45):kd<1.5?mix(vec3(.85,.9,1.),mix(vec3(1.,.6,.3),vec3(.9,.25,.3),rr),smoothstep(.05,.5,rr))*(.25+.2/(rr+.05)):vec3(1.,.93,.82)*den;
   if(ph(j*.9)>.97)col=kd<.5?vec3(1.,.3,.6)*2.:vec3(.6,.8,1.)*2.5;                  // pink knots, blue sparks
   if(kd>1.5){vec3 w=p-C,s=normalize(U[18].xyz);float bb=dot(w,s);col*=bb<0.&&dot(w,w)-bb*bb<U[16].w*U[16].w?.08:1.;}  // the planet's shadow
   col*=den*(.3+ph(j*1.7)*.9)*U[69].w*(1.-smoothstep(.85,1.,rr));
   sz=kd<.5?R*.002:kd<1.5?R*.00025:.05;
  }
 }else if(mode<5.5){                             // 5: the contact, then the world dissolves into points of light
  if(i<1.5){                                     // a blue point under her finger, then a red one
   p=U[13].xyz+vec3(i*.012-.006,.004-i*.008,-.012);
   float k=i<.5?U[15].x:U[15].y;
   col=(i<.5?vec3(.25,.5,1.):vec3(1.,.25,.15))*30.*clamp(k*4.,0.,1.)*step(0.,k)*(1.-clamp(U[14].x,0.,1.));sz=.004;
  }else if(i<12002.){                            // the pulse runs up her arm
   float k=r.x;vec3 W=U[30].xyz,E=U[29].xyz,S=U[28].xyz;
   p=(k<.5?mix(W,E,k*2.):mix(E,S,k*2.-1.))+normalize(ph3(i*1.7)-.5)*(.03+.01*r.z);
   col=mix(vec3(.25,.5,1.),vec3(1.,.25,.15),step(.5,r.y))*4.*exp(-pow((k-U[15].z)*8.,2.))*step(0.,U[15].z);sz=.002;
  }else{                                         // every visible surface of the frozen frame becomes a point of light
   vec2 ts=vec2(textureSize(SD,0));float j=i-12002.,st=sqrt(ts.x*ts.y/(U[63].w-12002.)),cl=floor(ts.x/st);
   vec2 g=vec2(mod(j,cl),floor(j/cl))*st+r.xy*st;
   float dz=texelFetch(SD,ivec2(g),0).x;vec3 sc=texelFetch(SC,ivec2(g),0).rgb;
   vec3 cp=U[18].xyz,f=normalize(U[19].xyz-cp),rt=normalize(cross(f,vec3(0,1,0))),up=cross(rt,f);
   rt=rt*cos(U[19].w)+up*sin(U[19].w);up=cross(rt,f);
   vec2 uv=(g/ts*2.-1.)*vec2(2.353,1);
   vec3 sp=cp+normalize(f/tan(radians(U[18].w)*.5)+uv.x*rt+uv.y*up)*dz;
   float rel=length(sp-U[13].xyz)/6.+r.z*.15,tt=max(U[14].x-rel*2.,0.);     // a front sweeping out from the touch
   p=sp+drift(sp*.3,t)*tt*.35+normalize(sp-U[13].xyz+1e-3)*tt*.05+vec3(0,tt*.3+tt*tt*.06,0);   // they rise like embers
   vec3 wc=mix(vec3(.25,.5,1.),vec3(1.,.25,.15),step(.5,ph(i*.37)));
   float sk=pow(ph(i*.53),3.)*3.+.25;                                       // sparkle (energy kept on average)
   col=mix(sc*1.3+.015,wc*.8,clamp(tt*.3,0.,.6))*sk*(.75+.25*sin(t*(3.+ph(i*.2)*6.)+i))
      *smoothstep(0.,.3,U[14].x-rel*2.)*(1.-smoothstep(2.,4.5,tt))*(1.-clamp((U[14].x-5.6)/1.,0.,1.));
   inten*=step(dz,500.)*step(g.y,ts.y);en=st*st;sz=.005+.008*ph(i*.91);
  }
 }
 pline=0.;
 if(i>=1e7){                                    // streak pass: line segments stretched by the speed
  float j=floor((i-1e7)*.5),tail=mod(i,2.);vec3 c=U[1].xyz;float E=60.;vec3 rr=ph3(j*.173);
  p=(rr-.5)*2.*E;p.z+=t*U[19].x*60.;p=c+mod(p-c+E,2.*E)-E;p.z-=tail*U[19].x*(8.+10.*rr.z);
  col=tail>.5?mix(vec3(.5,.15,.9),vec3(1.,.3,.35),rr.x)*.06:mix(vec3(.75,.85,1.),vec3(1.,.85,.9),rr.x)*(.6+.8*rr.y);
  sz=.02;pline=1.;inten=1.;
 }
 vec3 cp=U[1].xyz,fw=normalize(U[2].xyz-cp),rt=normalize(cross(fw,vec3(0,1,0))),up=cross(rt,fw);
 rt=rt*cos(U[2].w)+up*sin(U[2].w);up=cross(rt,fw);
 vec3 v=p-cp;float z=dot(v,fw),th=tan(radians(U[1].w)*.5);
 gl_Position=z>.01?vec4(dot(v,rt)/(th*2.353),dot(v,up)/th,0,z):vec4(2,2,2,1);
 pdist=length(v);
 float px=sz/(z*th)*U[0].z*.5;                  // radius in pixels (U[0].z: internal height)
 float coc=U[3].y*U[0].z*abs(pdist-U[3].x)/max(pdist,.01);
 float s=max(max(px,coc),1.);
 psz=s;
 gl_PointSize=s*2.;
 pc=vec4(col*inten*(en>0.?en/(s*s):min(1.,px*px/(s*s)+.02))*(z>.01?1.:0.),1);   // en: pixels of the frozen frame it carries
}
