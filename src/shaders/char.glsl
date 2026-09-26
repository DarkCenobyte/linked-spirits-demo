// ---- characters: heads, faces, eyes ------------------------------------------
// head space: origin at the centre of the head at eye height, +z = face forward, +y up.
// material ids: 1 composite skin, 2 eye, 3 metal, 4 lip, 5 emissive, 6 cable, 7 worn composite
float gM;             // material of the last evaluated sdf
float eyeBall(vec3 e);
vec3 gEyeC;           // centre of the closest eye (head space)
float gEyeSide;       // -1 right eye (image left when facing camera), +1 left eye
#define ER .0132      // eyeball radius
#define EC vec3(.033,0,.0648) // eye centre (right side, mirrored)
#define KU 2030.
#define KL 1680.

// groove of width w and depth dp carved into a surface d along the zero set of g
float groove(float d,float g,float w,float dp){return max(d,min(w-abs(g),d+dp));}

// face parameters: blink (0 open .. 1 closed), mouth open, mouth round, style (0 android, 1 cyborg)
float face(vec3 p0,vec2 bl2,float mo,float mr,float sty){
 // jaw: the lower face turns about a hinge under the ears. The angular gap it opens
 // between the lips is folded onto the lip line, then carved out as the mouth.
 vec2 v=p0.yz-vec2(-.016,-.012);
 float ja=mo*.18,jm=ja*smoothstep(-.012,.03,p0.z),a=atan(v.x,v.y);
 // sharp fold between the lips, a smooth stretch of the cheeks beyond the mouth corners
 float a2=a+mix(clamp(-.325-a,0.,jm),jm*smoothstep(-.2,.2,-.325-jm*.5-a),smoothstep(.016,.03,abs(p0.x)));
 vec3 p=vec3(p0.x,vec2(-.016,-.012)+length(v)*vec2(sin(a2),cos(a2)));
 vec3 q=p;q.x=abs(q.x);float blink=p.x>0.?bl2.y:bl2.x;
 // helmet-like cranium and an oval face mask tapering to the chin
 float d=ell(p-vec3(0,.03,-.02),vec3(.067,.085,.09));
 d=smin(d,ell(p-vec3(0,-.008,.024),vec3(.062,.089,.066)),.025);
 // jaw: two planes converging to the chin (V line), a defined underside, a small chin
 d=smax(d,dot(q-vec3(.05+.006*sty,-.056,.03),normalize(vec3(1,-.65+.2*sty,.28))),.018-.006*sty);
 d=smax(d,dot(p-vec3(0,-.093,.05),normalize(vec3(0,-1,.4))),.01);
 d=smin(d,ell(p-vec3(0,-.079-.003*sty,.055),vec3(.02+.007*sty,.013,.016)),.012);
 // planes: forehead, cheeks, temples (sculpted rather than round)
 d=smax(d,dot(p-vec3(0,.04,.082),normalize(vec3(0,.28,1))),.03);
 d=smax(d,dot(q-vec3(.05,-.024,.057),normalize(vec3(1,-.08,.62)))-.002*sty,.024);
 d=smax(d,q.x-.066,.014);
 // a shallow hollow under each cheekbone (finer face, no crease)
 vec2 cv=(q.xy-vec2(.047,-.043))/vec2(.015,.022);
 d+=.0024*exp(-dot(cv,cv))*smoothstep(.02,.05,p.z);
 // barely-there eye sockets
 d=smax(d,-ell(q-vec3(.033,.008,.086),vec3(.019,.009,.007)),.014);
 // face plate seam: over the forehead, down in front of the ears, under the jaw
 float g=(p.z-.033-.4*p.y-2.*p.y*p.y)*.9;
 d=groove(d,g,.0007,.0012);
 // cranial plates: central seam and a transverse seam over the crown
 if(g<-.004){d=groove(d,q.x,.0006,.001);d=groove(d,p.z+.02-p.y*.12,.0006,.001);}
 // thin lips: a subtle bulge split by the lip line (rounding narrows and pushes them)
 float mw=.02-mr*.0065;
 d=smin(d,ell(p-vec3(0,-.048,.077-mr*.001),vec3(mw+.001,.0055,.006+mr*.003)),.005);
 // the opening, in real space: from the upper lip down to the lowered lower lip
 float op=.098*ja,d0=d,xx=p0.x*p0.x/(mw*mw);
 float sl=ell(p0-vec3(0,-.048-op*.5+op*.15*xx-.0025*sty*xx,.09-20.*p0.x*p0.x),vec3(mw,.0006+op*.5+mr*.002,.02));
 d=smax(d,-sl,.0015);
 gM=d>d0+.0003?9.:abs(p.y+.048)<.0035&&q.x<mw-.001&&p.z>.07?4.:1.;
 // teeth: an upper plate fixed to the skull, a lower one carried by the jaw
 float tz=.0715-24.*p0.x*p0.x,te=min(box(vec3(p0.x,p0.y+.0512,p0.z-tz),vec3(.012,.0022,.003),.0015),
  box(vec3(p.x,p.y+.0522,p.z-tz+.001),vec3(.011,.002,.003),.0015));
 if(te<d){d=te;gM=10.;}
 // eyelids: shells around the eyeballs, opened by angular planes (almond shaped)
 vec3 e=q-EC;
 float le=length(e),sh=abs(le-ER-.0014)-.0011,sl2=abs(le-ER-.0009)-.0007;
 float au=mix(.5,-.5,blink)-KU*e.x*e.x+7.*e.x, al=-.42+KL*e.x*e.x+7.*e.x;
 float lu=smax(sh,-dot(e,vec3(0,cos(au),-sin(au))),.0009);
 float lo=smax(sl2,dot(e,vec3(0,cos(al),-sin(al))),.0007);
 float lid=max(min(lu,lo),-e.z-.006);
 d=smin(d,lid,.004);
 // eyeballs (real, un-mirrored side so the gaze is shared)
 gEyeSide=sign(p.x+1e-6);
 gEyeC=EC*vec3(gEyeSide,1,1);
 float eb=eyeBall(p-gEyeC);
 if(eb<d){d=eb;gM=2.;}
 // ear modules (headphone-like), crown bolts
 vec3 eq=(q-vec3(.063,-.008,-.012)).yxz;
 float ear=max(cyl(eq,.021,.006)-.0015,-cyl(eq-vec3(0,.007,0),.013,.003));
 ear=min(ear,cyl(eq,.009,.0075)-.001);
 if(sty>0.){ // cable connectors where the ears would be
  vec3 k=eq;k.xz=abs(k.xz);
  ear=min(ear,cap(q,vec3(.06,-.012,-.02),vec3(.083,-.024,-.042),.0075));
  ear=min(ear,cap(q,vec3(.062,.004,-.01),vec3(.088,.012,-.032),.0065));
  ear=min(ear,cap(q,vec3(.058,-.03,-.03),vec3(.078,-.048,-.05),.0085));}
 float bo=sph(q-vec3(.02,.103,.018),.0022);
 if(min(ear,bo)<d){d=min(ear,bo);gM=3.;}
 return d;
}
// ---- body: skeleton joints in U[B..B+19] (world space) ----------------------------
// segment with gaps at the joints so the metal articulations show
float seg(vec3 p,vec3 a,vec3 b,float ra,float rb,float ga,float gb){
 vec3 d=normalize(b-a);return capr(p,a+d*ga,b-d*gb,ra,rb);}
float hand(vec3 p,vec3 W,vec3 H,vec3 pn,float curl){
 vec3 hx=normalize(H-W+1e-5),hz=normalize(pn-hx*dot(pn,hx)+vec3(0,1e-4,0)),hy=cross(hz,hx);
 vec3 l=vec3(dot(p-W,hx),dot(p-W,hy),dot(p-W,hz));
 float d=box(l-vec3(.052,0,0),vec3(.042,.036,.011),.01);
 for(int i=0;i<4;i++){
  float fi=float(i),y=(fi-1.5)*.0185,L=(.042+.006*(1.-abs(fi-1.3)*.5));
  vec3 a=vec3(.092,y,.0),b=a+vec3(cos(curl*.9),0,-sin(curl*.9))*L;
  vec3 c=b+vec3(cos(curl*1.9),0,-sin(curl*1.9))*L*.8;
  d=smin(d,min(cap(l,a,b,.0082),cap(l,b,c,.0072)),.006);
 }
 vec3 t0=vec3(.025,.035,-.005),t1=t0+normalize(vec3(.5,.55,-.35))*.04,t2=t1+normalize(vec3(.9,.25,-.3-curl*.5))*.032;
 d=smin(d,min(cap(l,t0,t1,.011),cap(l,t1,t2,.0085)),.008);
 return d;
}
float body(vec3 p,int B){
 vec3 pel=U[B].xyz,chs=U[B+1].xyz,nk=U[B+2].xyz,hd=U[B+3].xyz;
 float bb=length(p-pel-(chs-pel)*.3)-1.05;
 if(bb>.05)return bb;
 vec3 up=normalize(chs-pel),sd=normalize(U[B+4].xyz-U[B+8].xyz),fw=normalize(cross(sd,up));
 sd=cross(up,fw);
 vec3 tp=vec3(dot(p-pel,sd),dot(p-pel,up),dot(p-pel,fw)),tq=vec3(abs(tp.x),tp.yz);
 float m=1.;
 float d=ell(tp-vec3(0,.0,-.01),vec3(.16,.12,.1));                 // hips
 d=smin(d,ell(tp-vec3(0,.17,0),vec3(.11,.12,.075)),.07);           // waist
 d=smin(d,ell(tp-vec3(0,.32,0),vec3(.145,.125,.09)),.06);          // rib cage
 d=smin(d,ell(tq-vec3(.058,.31,.062),vec3(.052,.05,.042)),.03);     // bust
 d=smin(d,capr(tq,vec3(.035,.412,-.015),vec3(.15,.425,-.01),.034,.045),.045); // shoulders: sloping from the neck
 d=smin(d,capr(p,nk-up*.05,hd-up*.055-fw*.022,.036,.031),.014);   // neck: slimmer than the head, set under the skull
 // neck ring (metal) + a green identity light
 float nr=tor(vec3(dot(p-nk,sd),dot(p-nk,up)-.01,dot(p-nk,fw)+.004),.0352,.0018);
 if(nr<d){d=nr;m=3.;}
 for(int s=0;s<2;s++){
  int o=B+4+s*4;
  vec3 S=U[o].xyz,E=U[o+1].xyz,W=U[o+2].xyz,H=U[o+3].xyz;
  float a=seg(p,S,E,.041,.032,.05,.03);
  a=min(a,seg(p,E,W,.03,.022,.03,.018));
  if(length(p-W)<.2)a=min(a,hand(p,W,H,U[60+s].xyz,U[60+s].w));
  d=smin(d,a,.012);
  float jn=min(sph(p-S,.044),min(sph(p-E,.027),sph(p-W,.018)));
  o=B+12+s*4;
  vec3 Hp=U[o].xyz,K=U[o+1].xyz,A=U[o+2].xyz,To=U[o+3].xyz;
  float g=seg(p,Hp,K,.074,.047,0.,.04);
  g=min(g,seg(p,K,A,.045,.028,.035,.02));
  vec3 fd=normalize(To-A);
  g=smin(g,capr(p,A+fd*.01+vec3(0,-.035,0),To+vec3(0,.012,0),.026,.02),.02);
  d=smin(d,g,.025);
  jn=min(jn,min(sph(p-K,.042),sph(p-A,.026)));
  if(jn<d){d=jn;m=3.;}
 }
 gM=m;
 return d;
}
