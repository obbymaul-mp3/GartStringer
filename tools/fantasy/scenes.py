import numpy as np, math, random
from PIL import Image
N=240
GHOSTS=True      # paint the dungeon's ghosts and the forest's fireflies into the picture (the firmware draws its own, so that they move)
FLIES=True
BAYER=np.array([[0,8,2,10],[12,4,14,6],[3,11,1,9],[15,7,13,5]],dtype=np.float32)/16
Y,X=np.mgrid[0:N,0:N].astype(np.float32)
def fin(a): 
    q=np.floor(np.clip(a,0,1)*31+np.tile(BAYER,(N//4,N//4))[...,None])/31
    return Image.fromarray((np.clip(q,0,1)*255).astype(np.uint8),'RGB')
def V(*c): return np.array(c,dtype=np.float32)
def sky(a,rnd,top,mid,hor,h=150,stars=150):
    t=np.clip(Y/h,0,1)[...,None]; a[:]=np.where(t<.55,top+(mid-top)*(t/.55),mid+(hor-mid)*((t-.55)/.45))
    for _ in range(stars):
        x,y=rnd.randrange(N),rnd.randrange(int(h*.75)); a[y,x]=V(1,1,.9)*rnd.uniform(.5,1)
def moon(a,mx,my,r=24):
    a[np.hypot(X-mx,Y-my)<r]=[.98,.9,.86]
    for cx,cy,cr in ((-8,-6,6),(7,5,5),(-3,10,3)): a[np.hypot(X-mx-cx*r/24,Y-my-cy*r/24)<cr*r/24]=[.82,.72,.80]
def flame(a,x,y,s=1):
    for i,(c,w,h) in enumerate(((V(1,.45,.15),4,9),(V(1,.8,.3),3,6),(V(1,1,.85),1,3))):
        for j in range(int(h*s)):
            hw=w*s*(1-j/(h*s)); a[int(y-j),int(x-hw):int(x+hw)+1]=c
def glow(a,x,y,r,col,k=1.0):
    d=np.hypot(X-x,Y-y); g=np.clip(1-d/r,0,1)**2*k; a+=g[...,None]*col
def rect(a,x0,y0,x1,y1,c): a[max(0,y0):y1,max(0,x0):x1]=c
def brick(u,v,su=8,sv=4): 
    r=np.floor(v/sv); return np.where((np.floor((u+(r%2)*su/2)/su)+r)%2==0,1.0,.86)*np.where((v%sv<1)|(((u+(r%2)*su/2)%su)<1),.72,1)

def castle():
    rnd=random.Random(3); a=np.zeros((N,N,3),np.float32); fog=V(.50,.27,.56)
    sky(a,rnd,V(.04,.03,.20),V(.34,.14,.52),V(1.0,.56,.52)); moon(a,176,42)
    def ridge(base,amp,seed,col,fk):
        rr=random.Random(seed); ph=[rr.uniform(0,6.3) for _ in range(4)]
        h=base+sum(amp/(i+1)*np.sin(X[0]*(0.018*(i+1.3))+ph[i]) for i in range(4)); h=np.round(h/2)*2
        a[Y>h[None,:]]=V(*col)*(1-fk)+fog*fk
    ridge(118,14,1,(.20,.14,.42),.75); ridge(132,12,2,(.16,.11,.36),.5)
    hill=np.round(150-46*np.exp(-((X[0]-92)/70)**2)+6*np.sin(X[0]*.05)); mk=Y>hill[None,:]
    sh=np.clip(1.05-(X-40)/260,.55,1.1)[...,None]
    g=V(.16,.30,.30)*sh*np.clip(1.25-(Y-hill[None,:])[...,None]/110,.5,1.25); a[:]=np.where(mk[...,None],g,a)
    path=np.abs(X-(92+26*np.sin((Y-104)*.06)+(Y-104)*.25))<(1.5+(Y-104)*.07); a[mk&path&(Y>108)]=[.62,.50,.60]
    def tower(x0,x1,ytop,roof,pen=True):
        w=x1-x0; cx=(x0+x1)/2
        for x in range(x0,x1):
            l=.55+.75*math.cos(((x-x0+.5)/w-.3)*2.2)**2
            for y in range(ytop,int(hill[x])+2):
                v=(y-ytop)/60; tint=V(1.2,1.02,.98)*(1-v)+V(.62,.56,.95)*v
                br=.9 if ((y//4)+((x+(y//4)%2*3)//6))%2 else 1.0
                a[y,x]=V(.42,.36,.56)*l*tint*br
        for y in range(ytop-roof,ytop):
            hw=(y-(ytop-roof))/max(1,roof)*(w/2+3)
            for x in range(int(cx-hw),int(cx+hw)+1): a[y,x]=V(.40,.20,.70)*(1.25 if x<cx else .7)
        if pen and roof:
            a[ytop-roof-8:ytop-roof,int(cx)]=[.9,.85,.8]
            for i in range(7): a[ytop-roof-8+abs(i-3)//2:ytop-roof-4-abs(i-3)//3,int(cx)+1+i]=[1,.37,.70]
    tower(74,112,76,0,False)
    for x in range(74,112,6): a[72:76,x:x+3]=a[80,x]
    tower(62,78,62,16); tower(108,124,58,18); tower(86,100,44,20)
    for x,y in ((90,54),(95,60),(68,74),(114,70),(80,86),(104,88)): rect(a,x,y,x+2,y+4,[1,.8,.4])
    rect(a,89,96,97,108,[1,.8,.4]); a[96,89]=a[96,96]=a[90,84]
    for px_,py_,s in ((24,150,16),(40,158,20),(150,146,14),(168,154,18),(204,160,22),(12,172,24),(228,176,24)):
        for i in range(s):
            hw=(i%(s//3+1))*.9+1+i*.22; yy=py_-s+i
            for x in range(int(px_-hw),int(px_+hw)+1):
                if 0<=x<N: a[yy,x]=V(.07,.17,.20)*(1.5 if x<px_ else .9)
    return fin(a)

def shrine():
    rnd=random.Random(8); a=np.zeros((N,N,3),np.float32)
    sky(a,rnd,V(.03,.05,.22),V(.20,.16,.50),V(.95,.50,.58),h=120); moon(a,34,34,16)
    # far castle on the horizon
    for x0,h in ((180,10),(186,16),(193,8)): rect(a,x0,118-h,x0+5,120,[.30,.18,.46])
    rect(a,176,112,204,120,[.30,.18,.46])
    sea=Y>=120; d=(Y-120)/120
    c=V(.10,.12,.40)*(1-d[...,None])+V(.03,.04,.16)*d[...,None]
    band=((Y+np.sin(X*.09+Y*.5)*1.5)%6<1.2)[...,None]*.10
    refl=(np.abs(X-34)<(3+ (Y-120)*.18))&((Y%3)<1.6)
    a[:]=np.where(sea[...,None],c+band,a); a[sea&refl]=[.85,.72,.80]
    # the island
    top=np.round(176-34*np.exp(-((X[0]-124)/66)**2)); isl=(Y>top[None,:])&(np.abs(X-124)<118-(Y-150)*0)
    isl&=(Y<226+0*X)
    g=V(.16,.30,.30)*np.clip(1.2-(X-30)/300,.6,1.2)[...,None]*np.clip(1.2-(Y-top[None,:])[...,None]/70,.45,1.2)
    a[:]=np.where((Y>top[None,:])[...,None],g,a)
    # broken arch: two pillars, the left one whole, an arc that stops short
    def pillar(x0,x1,y0,y1):
        for x in range(x0,x1):
            l=.6+.7*math.cos(((x-x0+.5)/(x1-x0)-.3)*2.2)**2
            for y in range(y0,y1): a[y,x]=V(.50,.44,.62)*l*(.86 if (y//5)%2 else 1)
    pillar(82,94,84,150); pillar(154,166,104,150); rect(a,79,80,97,85,V(.62,.56,.72)); rect(a,152,100,168,105,V(.5,.44,.6))
    for t in np.linspace(math.pi,math.pi*.42,160):
        cx,cy=124+36*math.cos(t),86-34*math.sin(t)
        rect(a,int(cx)-3,int(cy)-3,int(cx)+3,int(cy)+3,V(.56,.50,.68)*(1.1 if t>2.2 else .85))
    # the stone and the sword
    for y in range(134,152):
        hw=6+(y-134)*.8; a[y,int(124-hw):int(124+hw)]=V(.36,.32,.48)*(1.25-(y-134)/40)
    rect(a,123,96,126,136,[.62,.70,.91]); rect(a,123,96,124,136,[1,1,1])
    rect(a,117,104,132,107,[.95,.76,.31]); rect(a,123,88,126,104,[.95,.76,.31]); rect(a,123,92,126,96,[1,.37,.70])
    for bx in (60,188):
        rect(a,bx-1,150,bx+2,166,[.20,.16,.30]); rect(a,bx-5,146,bx+6,150,[.30,.24,.40]); flame(a,bx,146,1.2)
        glow(a,bx,142,46,V(1.2,.6,.25),.55)
    glow(a,124,94,26,V(1.2,.3,.8),.5)
    return fin(a)

def corridor(vx,vy,hw,hh,zmax):
    dx=(X-vx)/120; dy=(Y-vy)/120
    tw=hw/np.maximum(np.abs(dx),1e-4); tf=hh/np.maximum(np.abs(dy),1e-4); z=np.minimum(tw,tf)
    wall=tw<tf; end=z>zmax; z=np.minimum(z,zmax)
    return dx,dy,z,wall,end
def dungeon():
    rnd=random.Random(4); a=np.zeros((N,N,3),np.float32); fog=V(.10,.05,.20)
    dx,dy,z,wall,end=corridor(120,104,1.0,.72,9.0)
    u=z*30; vw=dy*z*60; vf=dx*z*60
    tex=np.where(wall,brick(u,vw+200,10,5),np.where(dy>0,brick(vf+300,u,9,9),brick(u,vf+300,12,6)*.8))
    base=np.where(wall[...,None],V(.36,.30,.50),np.where((dy>0)[...,None],V(.30,.26,.42),V(.22,.18,.34)))*tex[...,None]
    light=np.zeros_like(z)+.22
    for zk in (1.6,3.6,5.6,7.6): light+=1.5*np.exp(-((z-zk)/.55)**2)*np.where(wall,1,.55)
    col=base*(V(.16,.13,.27)*2+light[...,None]*V(1.5,.8,.35)*.8)
    k=np.clip(z/9.5,0,1)[...,None]**1.4; col=col*(1-k)+fog*k
    arch=(np.abs((z%2.0)-0.6)<.07)&~end; col[arch]*=.35
    col[end]=fog*.6; a[:]=col
    # a far doorway with the heart's light in it
    rect(a,114,96,127,113,[.03,.01,.08]); glow(a,120,104,30,V(1.5,.3,.9),.7); rect(a,119,102,122,105,[1,.25,.6])
    for zk,side in ((1.6,-1),(1.6,1),(3.6,-1),(3.6,1),(5.6,-1),(5.6,1)):
        x=120+side*120/zk; y=104-.18*120/zk; s=2.2/zk
        rect(a,int(x-1.5*s),int(y),int(x+1.5*s)+1,int(y+7*s),[.12,.08,.16]); flame(a,x,y,s)
        glow(a,x,y-3*s,60/zk+8,V(1.3,.65,.25),.5)
    # chains
    for cx in (34,206):
        for y in range(0,70,4): rect(a,cx,y,cx+2,y+3,[.30,.28,.40]); rect(a,cx+ (1 if (y//4)%2 else -1),y+2,cx+2+(1 if (y//4)%2 else -1),y+4,[.18,.16,.26])
    # ghosts: pale, a little see-through, with a ragged hem
    def ghost(cx,cy,s,al):
        body=((np.hypot((X-cx)/s,(np.minimum(Y,cy)-cy)/(s*1.1))<1)&(Y<cy+s*1.5+np.round(np.sin((X-cx)/s*5)*s*.22)))&(np.abs(X-cx)<s)
        a[body]=a[body]*(1-al)+V(.80,.76,1.0)*al
        hi=body&(X<cx-s*.35)&(Y<cy); a[hi]=a[hi]*.5+V(1,1,1)*.5
        for e in (-.38,.38): rect(a,int(cx+e*s-s*.14),int(cy-s*.2),int(cx+e*s+s*.14)+1,int(cy+s*.22)+1,[.08,.03,.2])
        rect(a,int(cx-s*.12),int(cy+s*.45),int(cx+s*.12)+1,int(cy+s*.7)+1,[.08,.03,.2])
        glow(a,cx,cy,s*2.6,V(.5,.45,.9),.25)
    if GHOSTS: ghost(176,84,15,.72); ghost(66,112,10,.6); ghost(133,98,5,.5)
    return fin(a)

def cathedral():
    a=np.zeros((N,N,3),np.float32); fog=V(.12,.06,.24)
    dx,dy,z,wall,end=corridor(120,112,1.25,.95,7.0)
    xf=dx*z; floor=(~wall)&(dy>0); ceil=(~wall)&(dy<=0)
    chk=((np.floor(xf*3+50)+np.floor(z*3))%2)
    fcol=np.where(chk[...,None]>0,V(.62,.56,.74),V(.20,.16,.34)); carpet=floor&(np.abs(xf)<.34)
    fcol=np.where(carpet[...,None],V(.78,.14,.52)*np.where((np.abs(xf)>.28)[...,None],V(1.2,1.1,.5),1),fcol)
    vw=dy*z
    colm=(np.abs((z%1.5)-.5)<.16); archy=(vw<-.35-0.35*np.abs(np.cos((z%1.5)/1.5*math.pi)))
    wcol=np.where(colm[...,None],V(.56,.50,.70),V(.24,.19,.40))*brick(z*40,vw*40+200,10,6)[...,None]
    wcol=np.where((archy&~colm)[...,None],V(.14,.10,.26),wcol)
    win_=(~colm)&(vw>-.3)&(vw<.25)&(np.abs((z%1.5)-1.1)<.14); 
    glass=np.stack([.5+.5*np.sin(z*9+vw*14),.3+.3*np.sin(z*5+2+vw*9),.6+.4*np.sin(vw*20+z)],-1)
    wcol=np.where(win_[...,None],glass*1.1,wcol)
    ccol=V(.16,.12,.30)*(1-.3*((np.floor(z*2)%2))[...,None])
    col=np.where(wall[...,None],wcol,np.where(floor[...,None],fcol,ccol))
    light=.55+.5*np.exp(-((z-2.5)/2.5)**2); col=col*light[...,None]*V(1.05,.95,1.1)
    k=np.clip(z/8.5,0,1)[...,None]**1.5; col=col*(1-k)+fog*k; lit=wall&win_; col[lit]=np.maximum(col[lit],glass[lit]*.75)
    col[end]=V(.13,.09,.26); a[:]=col
    # the end wall: rose window and the organ under it
    ex0,ex1,ey0,ey1=int(120-1.25*120/7),int(120+1.25*120/7),int(112-.95*120/7),int(112+.95*120/7)
    r=np.hypot(X-120,Y-106); th=np.arctan2(Y-106,X-120)
    pal=[V(.95,.76,.31),V(.48,.42,1),V(1,.37,.70),V(.31,.84,.78),V(1,.54,.29)]
    sec=np.floor((th+math.pi)/(2*math.pi)*10).astype(int)%5
    for i in range(5):
        m=(r<8)&(r>2)&(sec==i)&(np.abs(((th+math.pi)/(2*math.pi)*10)%1-.5)<.4); a[m]=pal[i]
    a[(r<=2)]=[1,.25,.6]; a[(r>=8)&(r<9.5)]=[.62,.56,.74]
    for i,x in enumerate(range(ex0+3,ex1-2,3)):
        h=6+4*abs(math.sin(i*1.1)); rect(a,x,ey1-int(h)-1,x+2,ey1-1,V(.95,.76,.31)*(1 if i%2 else .75))
    glow(a,120,106,34,V(1.2,.5,.9),.35)
    for zk in (1.2,2.7,4.2):
        for side in (-1,1):
            x=120+side*1.05*120/zk; y=112+.1*120/zk; s=2/zk
            flame(a,x,y,s*1.1); glow(a,x,y-3,40/zk+6,V(1.3,.7,.3),.4)
    return fin(a)

def forest():
    rnd=random.Random(11); a=np.zeros((N,N,3),np.float32)
    sky(a,rnd,V(.02,.06,.16),V(.08,.22,.36),V(.45,.80,.72),h=170,stars=110); moon(a,150,58,30)
    glow(a,150,58,90,V(.3,.6,.55),.5)
    for layer,(base,col,n) in enumerate(((150,V(.12,.26,.34),9),(168,V(.07,.17,.26),7),(190,V(.03,.09,.16),6))):
        a[Y>base+4*np.sin(X*.05+layer)]=col
        for i in range(n):
            px_=int((i+.5)*N/n+rnd.uniform(-10,10)); s=int(rnd.uniform(34,58)+layer*12)
            for j in range(s):
                hw=(j%(s//4+1))*.8+1+j*.2; yy=base-s+j+6
                x0,x1=max(0,int(px_-hw)),min(N,int(px_+hw)+1)
                if 0<=yy<N: a[yy,x0:x1]=col*(1.25 if j%(s//4+1)<2 else 1)
            rect(a,px_-1,base,px_+2,base+10,col*.7)
        mist=((Y>base-10)&(Y<base+8)&(((Y+layer)%3)<1))
        a[mist]=a[mist]*.6+V(.5,.8,.75)*.4
    for _ in range(26 if FLIES else 0):
        x,y=rnd.randrange(8,232),rnd.randrange(110,215); a[y,x]=[.8,1,.7]; glow(a,x,y,7,V(.45,1,.75),.5)
    return fin(a)

def forge():
    rnd=random.Random(21); a=np.zeros((N,N,3),np.float32); a[:]=V(.07,.03,.10)
    wallt=brick(X,Y,14,8); a[:]=V(.16,.08,.16)*wallt[...,None]*np.clip(.5+Y/240,0,1.3)[...,None]
    for i in range(14):                                                # stalactites
        px_=int(i*18+rnd.uniform(0,10)); s=int(rnd.uniform(14,46))
        for j in range(s): 
            hw=(1-j/s)*7; a[j,max(0,int(px_-hw)):int(px_+hw)+1]=V(.10,.05,.12)*(1.5 if j%5==0 else 1)
    lava=Y>186+4*np.sin(X*.07)+2*np.sin(X*.21)
    lc=np.where((((X+Y*2+np.sin(Y*.6)*6)%14)<5)[...,None],V(1,.85,.35),V(1,.38,.16)); lc=np.where((Y>205)[...,None],lc*V(1,.75,.8),lc)
    a[:]=np.where(lava[...,None],lc,a)
    g=np.clip(1-np.abs(Y-190)/95,0,1)**2; a+=g[...,None]*V(1.0,.28,.22)*.8*~lava[...,None]
    # the anvil on a rock shelf, a hammer, a hung chain
    for y in range(150,196):
        hw=44+(y-150)*.7; a[y,int(120-hw):int(120+hw)]=V(.14,.08,.14)*(1.6 if y<153 else 1)+V(.5,.12,.08)*((y-150)/46)**2
    A=V(.20,.16,.26)
    rect(a,96,122,150,131,A*1.5); rect(a,96,122,150,124,A*2.4); 
    for i in range(10): rect(a,150+i,123+i//2,151+i,129-i//3,A*1.5)
    rect(a,108,131,138,137,A); rect(a,114,137,132,145,A*.8); rect(a,104,145,142,150,A*1.2)
    rect(a,118,116,128,122,[1,.7,.3]); glow(a,123,118,28,V(1.4,.6,.2),.7)   # the glowing blade on the anvil
    rect(a,128,118,146,120,[1,.9,.6])
    for _ in range(40):
        x,y=rnd.randrange(N),rnd.randrange(60,186); a[y,x]=V(1,.7,.3) if rnd.random()<.6 else V(1,.37,.7)
    return fin(a)
