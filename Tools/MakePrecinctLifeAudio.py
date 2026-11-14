"""Original, deterministic phone and fan sounds. AI/python/python.exe -I Tools/MakePrecinctLifeAudio.py.
No recordings, downloads or licenses beyond the project's own assets.
"""
from pathlib import Path
import wave
import numpy as np

root=Path(__file__).resolve().parent/'StaffAudio'
root.mkdir(exist_ok=True)
rate=22050
def save(name,a):
    a=a/(max(abs(a).max(),.001))*0.6
    with wave.open(str(root/(name+'.wav')),'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(rate)
        w.writeframes((np.clip(a,-1,1)*32767).astype('<i2').tobytes())

# Two pairs of old desk-phone rings, then silence: an unanswered line never becomes a squad task.
t=np.arange(int(rate*4.2))/rate
a=np.zeros_like(t)
for start in (0,.8,2.5,3.3):
    u=t-start
    gate=np.clip(u/.008,0,1)*np.clip((.55-u)/.07,0,1)
    bell=np.sin(2*np.pi*460*u)+.6*np.sin(2*np.pi*620*u)+.18*np.sin(2*np.pi*1240*u)
    a+=bell*gate*(.65+.35*np.sin(2*np.pi*25*u))
save('SFX_OfficePhone',a)

# Periodic Fourier air noise makes an exact eight-second loop; low motor harmonics sit beneath it.
rng=np.random.default_rng(2005)
n=rate*8
freq=np.fft.rfftfreq(n,1/rate)
spectrum=rng.normal(size=len(freq))+1j*rng.normal(size=len(freq))
spectrum*=np.exp(-freq/700)*(1-np.exp(-freq/60))
air=np.fft.irfft(spectrum,n=n)
air/=max(abs(air).max(),.001)
t=np.arange(n)/rate
a=.45*air*(.85+.15*np.sin(2*np.pi*3*t))+.08*np.sin(2*np.pi*60*t)+.025*np.sin(2*np.pi*120*t)
save('AMB_OfficeFan',a)
print('Precinct life: two original mono WAVs in',root)
