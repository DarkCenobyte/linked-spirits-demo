from score import *
QUAL={'m':[0,3,7],'M':[0,4,7],'7':[0,4,7,10],'s':[0,5,7,10],'2':[0,2,7],'m9':[0,3,7,10,2],'M7':[0,4,7,11],'m6':[0,3,7,9]}
def chord(bar):
    c=CH.get(bar,'Dm'); r=c[0]; i=1
    if len(c)>1 and c[1] in '#b': r+=c[1]; i=2
    root=(m(r+'4'))%12; q=c[i:] or 'M'
    return root,[(root+x)%12 for x in QUAL[q]],c
names='C C# D Eb E F F# G Ab A Bb B'.split()
issues=0
for v in VOX:
    for notes,who in ((v[2],v[0][0]),(v[3],'B') if v[3] else (None,None)):
        if notes is None: continue
        for p,s,l in notes:
            bar=v[1]+s//16; pos=s%16
            root,tones,c=chord(bar)
            if p%12 not in tones and (l>=4 or pos in (0,8)):
                print('bar %3d pos %2d len %2d %-3s %s over %s'%(bar,pos,l,names[p%12],who,c)); issues+=1
    if v[3]:
        A=v[2]; B=v[3]
        for t in range(0,64):
            a=[p for p,s,l in A if s<=t<s+l]; b=[p for p,s,l in B if s<=t<s+l]
            if a and b:
                iv=(a[0]-b[0])%12
                if iv in (1,2,6,10,11) and t%4==0: print('  duo bar %d t%d interval %d (%s/%s)'%(v[1]+t//16,t,iv,names[a[0]%12],names[b[0]%12])); issues+=1
print(issues,'issues')
