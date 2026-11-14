"""Build only the briefing lectern and projector frustum; run with AI/python/python.exe -I.
Uses the existing mesh kit, centimetres, +X front. No downloaded assets.
"""
import os, sys
HERE=os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0,HERE)
from meshkit import Mesh, rot_y
from make_props import save

m=Mesh()
m.box('Wood',(-32,-40,0),(32,40,7),1)
m.box('Wood',(20,-36,7),(26,36,108),.6)
for y in (-36,31): m.box('Wood',(-24,y,7),(20,y+5,109),.6)
m.box('Wood',(-24,-31,62),(20,31,65),.4)
m.box('Wood',(-34,-41,110),(34,41,116),.8,xf=rot_y(-10,(0,0,113)))
m.box('Wood',(-32,-40,116),(-29,40,119),.5,xf=rot_y(-10,(0,0,113)))
m.box('Brass',(26,-14,58),(26.7,14,83),1)
m.box('Black',(26.7,-10,62),(27,10,79),.8)
save('BriefingPodium',m)

# Side faces of a rectangular projection frustum; open at both ends.
m=Mesh()
for scale in (.2,.3,.4,.5,.6,.7,.8,.9,1.0):
    a=[(227,-2*scale,350-scale),(227,2*scale,350-scale),(227,2*scale,350+scale),(227,-2*scale,350+scale)]
    b=[(778,-248*scale,230-136*scale),(778,248*scale,230-136*scale),(778,248*scale,230+136*scale),(778,-248*scale,230+136*scale)]
    for i in range(4):
        j=(i+1)%4
        m.poly('Water',[a[i],a[j],b[j],b[i]],(500,0,290))
save('ProjectorFrustum',m)

# Full-size early-2000s fleet sedan: bevelled sheet metal, sloped glazing and inset wheels.
m=Mesh()
m.box('Black',(-246,-94,38),(246,94,83),9)
m.box('Black',(72,-91,82),(232,91,93),4)
m.box('Black',(-236,-91,82),(-132,91,91),4)
m.hexa('Beige',[(-139,-87,82),(105,-87,82),(105,87,82),(-139,87,82),
                (-103,-75,142),(52,-75,142),(52,75,142),(-103,75,142)])
m.box('Black',(-103,-75,139),(52,75,147),3)
for side in (-1,1):
    y=side*89
    m.box('Cooler',(-130,min(y,side*96),48),(109,max(y,side*96),86),2)
    # Front and rear side windows follow the cabin trapezoid; pillars remain visible.
    for xa,xb,ta,tb in ((-129,-37,-99,-37),(-30,96,-30,48)):
        m.poly('Screen',[(xa,side*87.8,90),(xb,side*87.8,90),(tb,side*75.8,136),(ta,side*75.8,136)],(0,0,100))
    m.box('Black',(-35,min(side*96,side*97),49),(-33,max(side*96,side*97),88))
    for x in (-100,65): m.box('Chrome',(x-9,min(side*97,side*99),81),(x+9,max(side*97,side*99),84),.5)
    m.box('Black',(65,min(side*91,side*111),96),(90,max(side*91,side*111),107),2)
    for x in (-155,155):
        m.cylinder('Dark',(x,side*94,34),(0,side,0),34,20,segs=24)
        m.cylinder('Steel',(x,side*105,34),(0,side,0),24,1.5,segs=24)
        m.cylinder('Chrome',(x,side*106,34),(0,side,0),16,2,segs=16)
        m.cylinder('Black',(x,side*107.5,34),(0,side,0),9,2,segs=12)
    m.box('Paper',(246,min(side*45,side*85),65),(249,max(side*45,side*85),79),1)
    m.box('PinRed',(-249,min(side*45,side*85),62),(-246,max(side*45,side*85),78),1)
m.poly('Screen',[(104,-82,92),(104,82,92),(53,73,137),(53,-73,137)],(0,0,110))
m.poly('Screen',[(-138,82,91),(-138,-82,91),(-104,-73,136),(-104,73,136)],(0,0,110))
m.box('Chrome',(245,-88,43),(251,88,52),2)
m.box('Chrome',(-251,-88,43),(-245,88,52),2)
m.box('Dark',(246,-38,60),(250,38,80),1)
for y in range(-32,33,8): m.box('Chrome',(250,y-1,62),(251,y+1,77))
m.box('Black',(253,-60,35),(261,-54,81),2)
m.box('Black',(253,54,35),(261,60,81),2)
m.box('Black',(253,-60,46),(261,60,52),2)
m.box('Paper',(-252,-21,54),(-251,21,67),1)
m.box('Black',(-34,-74,148),(5,74,153),2)
m.box('PinRed',(-31,-70,153),(2,-3,164),2)
m.box('TapCold',(-31,3,153),(2,70,164),2)
m.cylinder('Steel',(-185,-64,92),(0,0,1),.6,48,segs=6)
save('FleetCruiser',m)
