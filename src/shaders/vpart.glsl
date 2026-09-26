// ---- particles: everything is generated from gl_VertexID ------------------------
// U[63]: x mode, y seed/phase, z intensity, w count.  U[16..19]: mode parameters.
out vec4 pc;          // colour (premultiplied intensity)
out float pdist;      // distance from the camera (for depth occlusion)
out float psz;        // sprite size in pixels
out float pline;      // 1 for streak lines
float ph(float n){return fract(sin(n*12.9898)*43758.5453);}
vec3 ph3(float n){return fract(sin(vec3(n,n+17.1,n+31.7)*12.9898)*43758.5453);}
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
 vec3 p=vec3(0),col=vec3(1);float sz=.004,inten=U[63].z;
 vec3 r=ph3(i*.137+U[63].y);
 if(mode<1.5){                                   // 1: motes of light floating in the room
  vec3 c=U[16].xyz;vec3 ext=vec3(U[16].w,U[16].w*.45,U[16].w);
  p=c+(r-.5)*2.*ext;if(U[17].y>.5)p=c+sdir(i*.77)*U[16].w*pow(r.x,.6);p+=drift(p,t)*.15;
  vec3 h=U[4].xyz-p;float dh=length(h);
  p+=h/dh*U[17].x*.3*exp(-dh*1.5)*sin(t*.7+i);        // they begin to react to her
  if(U[17].y<.5)p=c+mod(p-c+ext,2.*ext)-ext;
  col=mix(mix(vec3(.9,1.,.95),vec3(.3,1.,.5),step(.8,r.x)),mix(vec3(.2,.4,1.),vec3(1.,.15,.1),step(.5,r.z)),U[17].y)*(.5+.5*sin(t*(1.+r.y*2.)+i));
  sz=.006;
 }else if(mode<2.5){                             // 2: the first green lights in the dark
  float k=mod(i,4.);p=vec3((mod(k,2.)*2.-1.)*(k<2.?1.7:2.5),k<2.?1.45:2.1,k<2.?-1.55:-2.2);
  col=vec3(.2,1.,.45)*step(k+.5,U[12].y)*4.;sz=.02;
 }else if(mode<3.5){                             // 3: cyberspace
  float N=U[63].w,f=i/N;
  if(f<.6){                                      // bodies of light: blue android, red cyborg, and sometimes a third
   float who=step(.45,ph(i*.91));
   vec4 b=bodyPt(who>.5?40:20,i);
   p=b.xyz+drift(b.xyz*6.,t*1.5)*.01;
   inten*=step(-.5,b.w);
   col=b.w>2.5?(who>.5?vec3(.04,.22,1.)*1.7:vec3(.06,1.,.22)*1.3):who>.5?vec3(1.,.13,.08):vec3(.15,.4,1.);
   float th=U[19].w*step(ph(i*.37),.55);          // the climax: they interpenetrate
   if(th>0.){vec4 b2=bodyPt(who>.5?20:40,i);vec3 m=(b.xyz+b2.xyz)*.5;p=mix(p,m+drift(m*9.,t*3.)*.02,th);
    col=mix(col,vec3(1.,.95,1.)*(.8+.4*sin(t*20.+i)),th*.8)+vec3(1,0,0)*th*step(.8,ph(i*1.1))*sin(t*30.+i)+vec3(0,0,1)*th*step(.8,ph(i*1.3))*sin(t*27.+i);}
   col*=(b.w>2.5?.9+.2*r.z:.5+.9*r.z)*(who>.5?U[59].w:1.);sz=b.w>2.5?.0036:.0032;
  }else if(f<.92){                               // the field: dust of stars travelling past
   vec3 c=U[1].xyz;float E=40.;
   p=(r-.5)*2.*E;p.z-=t*U[19].x*25.;p=c+mod(p-c+E,2.*E)-E;
   col=mix(vec3(.6,.7,1.),vec3(1.,.7,.6),ph(i*.3))*(.3+ph(i*.7));sz=.03;
  }else{                                         // fugitive structures of light: rings, bridges, helices
   inten*=U[19].x>.01?1.:0.;
   float s=ph(i*.17),k=floor(t/9.6),tk=fract(t/9.6),shp=mod(k,3.);
   vec3 o=vec3(0,-6.+ph(k)*4.,-40.+tk*30.);
   if(shp<.5)p=o+vec3(cos(s*6.283)*25.,0,sin(s*6.283)*25.);
   else if(shp<1.5)p=o+vec3((s-.5)*80.,-2.+cos(s*3.14)*4.,sin(s*40.)*.3);
   else p=o+vec3(cos(s*60.)*6.,(s-.5)*60.,sin(s*60.)*6.);
   col=mix(vec3(.3,.5,1.),vec3(1.,.3,.4),ph(k*1.3))*sin(tk*3.14)*1.5;sz=.06;
  }
 }else if(mode<4.5){                             // 4: inside a planetary ring: an ocean of fragments
  vec3 c=U[1].xyz;float E=6.;
  p=(r-.5)*2.*vec3(E,E*.25,E);p.x-=t*1.2;p=c+mod(p-c+vec3(E,E*.25,E),2.*vec3(E,E*.25,E))-vec3(E,E*.25,E);
  float br=pow(ph(i*.9),3.);col=mix(vec3(.5,.45,.4),vec3(1.,.93,.85),br)*(.05+1.2*br);sz=.004+.03*pow(ph(i*.4),6.);
 }
 pline=0.;
 if(i>=1e7){                                    // streak pass: line segments stretched by the speed
  float j=floor((i-1e7)*.5),tail=mod(i,2.);vec3 c=U[1].xyz;float E=60.;vec3 rr=ph3(j*.173);
  p=(rr-.5)*2.*E;p.z-=t*U[19].x*60.;p=c+mod(p-c+E,2.*E)-E;p.z+=tail*U[19].x*6.;
  col=mix(vec3(.5,.6,1.),vec3(1.,.6,.7),rr.x)*.8;sz=.02;pline=1.;inten=1.;
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
 pc=vec4(col*inten*min(1.,px*px/(s*s)+.02)*(z>.01?1.:0.),1);
}
