out vec2 q;
void main(){vec2 p=vec2(gl_VertexID&1,gl_VertexID>>1)*4.-1.;q=p*.5+.5;gl_Position=vec4(p,0,1);}
