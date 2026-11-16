from pathlib import Path
import pefile,capstone,struct,re
pa=Path('lego FOOTBALL MANIA/Setup/English/program files/LEGO Media/Soccer Mania/Game.exe');pb=Path('Lego Football Mania Dutch/Game.exe')
a,b=pa.read_bytes(),pb.read_bytes();p=pefile.PE(data=a);base=p.OPTIONAL_HEADER.ImageBase
s=p.sections[0];start=s.PointerToRawData;end=start+s.SizeOfRawData
idx=[i for i in range(start,end) if a[i]!=b[i]];groups=[]
for i in idx:
 if not groups or i-groups[-1][-1]>16:groups.append([i])
 else:groups[-1].append(i)
for g in groups:
 lo,hi=g[0],g[-1]+1
 if set(a[lo:hi])=={0x90}:continue
 print('NONPADDING',hex(lo),hex(base+p.get_rva_from_offset(lo)),a[lo:hi].hex(),b[lo:hi].hex())
for match in re.finditer(rb'[ -~]{5,}',a):
 st=match.group()
 if any(k in st.lower() for k in [b'game.dat',b'cdilla',b'safedisc',b'finddisc',b'language',b'mania\\cd',b'no cd',b'cd-rom',b'cdrom',b'resolution']):
  va=base+p.get_rva_from_offset(match.start()); needle=struct.pack('<I',va)
  refs=[hex(base+p.get_rva_from_offset(m.start())) for m in re.finditer(re.escape(needle),a[:end])]
  print('STRING',hex(va),st[:160],refs[:12])
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
for va,size in [(0x533246,180),(0x401090,360)]:
 off=p.get_offset_from_rva(va-base)
 print('\nDISASM',hex(va))
 for i in md.disasm(a[off:off+size],va):print(hex(i.address),i.mnemonic,i.op_str)
