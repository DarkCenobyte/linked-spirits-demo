// ---- particles: everything is generated from gl_VertexID ------------------------
// U[63]: x mode, y seed/phase, z intensity, w count.  U[16..19]: mode parameters.
out vec4 pc;          // colour (premultiplied intensity)
out float pdist;      // distance from the camera (for depth occlusion)
out float psz;        // sprite size in pixels
float ph(float n){return fract(sin(n*12.9898)*43758.5453);}
vec3 ph3(float n){return fract(sin(vec3(n,n+17.1,n+31.7)*12.9898)*43758.5453);}
vec3 sdir(float n){vec3 r=ph3(n);float z=r.x*2.-1.,a=r.y*6.2831;return vec3(sqrt(1.-z*z)*vec2(cos(a),sin(a)),z);}
// gentle curl-like drift
vec3 drift(vec3 p,float t){return vec3(sin(p.y*1.3+t*.3+p.z),sin(p.z*1.1+t*.27+p.x*.7),sin(p.x*1.2+t*.23+p.y*.5));}
// a point on a body (joints at U[B..B+19]); returns the position, w = segment id
vec4 bodyPt(int B,float n){
 float s=ph(n*1.31)*20.;int k=int(s);vec3 r=ph3(n*.71);
 vec3 a,b;float ra,rb;
 if(k<5){a=U[B].xyz;b=U[B+2].xyz;ra=.14;rb=.12;}                        // torso
 else if(k<7){a=U[B+3].xyz;b=a;ra=rb=.105;}                               // head
 else if(k<13){int j=k<10?4:8;int m=(k-(k<10?7:10));a=U[B+j+min(m,2)].xyz;b=U[B+j+min(m+1,3)].xyz;ra=.04;rb=.028;}  // arms
 else{int j=k<17?12:16;int m=(k-(k<17?13:17))%3;a=U[B+j+m].xyz;b=U[B+j+m+1].xyz;ra=.065;rb=.04;}       // legs
 float u=r.x;vec3 c=mix(a,b,u);
 vec3 d=normalize(b-a+vec3(1e-4,0,0)),o=normalize(cross(d,sdir(n*3.3)));
 float rad=mix(ra,rb,u)*sqrt(r.y);
 if(k>=5&&k<7)o=sdir(n*5.7),rad=ra*(.7+.3*r.y);
 return vec4(c+o*rad,float(k));
}
void main(){
 float i=float(gl_VertexID),t=U[0].x,mode=U[63].x;
 vec3 p=vec3(0),col=vec3(1);float sz=.004,inten=U[63].z;
 vec3 r=ph3(i*.137+U[63].y);
 if(mode<1.5){                                   // 1: motes of light floating in the room
  vec3 c=U[16].xyz;vec3 ext=vec3(U[16].w,U[16].w*.45,U[16].w);
  p=c+(r-.5)*2.*ext;p+=drift(p,t)*.15;
  vec3 h=U[4].xyz-p;float dh=length(h);
  p+=h/dh*U[17].x*.3*exp(-dh*1.5)*sin(t*.7+i);        // they begin to react to her
  p=c+mod(p-c+ext,2.*ext)-ext;
  col=mix(vec3(.9,1.,.95),vec3(.3,1.,.5),step(.8,r.x))*(.5+.5*sin(t*(1.+r.y*2.)+i));
  sz=.006;
 }else if(mode<2.5){                             // 2: the first green lights in the dark
  float k=mod(i,4.);p=vec3((mod(k,2.)*2.-1.)*(k<2.?1.7:2.5),k<2.?1.45:2.1,k<2.?-1.55:-2.2);
  col=vec3(.2,1.,.45)*step(k+.5,U[12].y)*4.;sz=.02;
 }else if(mode<3.5){                             // 3: bodies of light (astral): two bodies, blue and red
  float who=step(.5,ph(i*.91));
  vec4 b=bodyPt(who>.5?40:20,i);
  p=b.xyz+drift(b.xyz*6.,t*1.5)*.012;
  col=who>.5?vec3(1.,.15,.1):vec3(.2,.45,1.);
  col*=.6+.8*r.z;sz=.0035;
 }
 vec3 cp=U[1].xyz,fw=normalize(U[2].xyz-cp),rt=normalize(cross(fw,vec3(0,1,0))),up=cross(rt,fw);
 rt=rt*cos(U[2].w)+up*sin(U[2].w);up=cross(rt,fw);
 vec3 v=p-cp;float z=dot(v,fw),th=tan(radians(U[1].w)*.5);
 gl_Position=z>.01?vec4(dot(v,rt)/(th*2.353),dot(v,up)/th,0,z):vec4(2,2,2,1);
 pdist=length(v);
 float px=sz/(z*th)*408.;                        // radius in pixels at 816 lines
 float coc=U[3].y*816.*abs(pdist-U[3].x)/max(pdist,.01);
 float s=max(max(px,coc),1.);
 psz=s;
 gl_PointSize=s*2.;
 pc=vec4(col*inten*min(1.,px*px/(s*s)+.02)*(z>.01?1.:0.),1);
}
