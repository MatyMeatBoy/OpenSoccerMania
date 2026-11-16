from pathlib import Path
import pefile,capstone,collections
paths=[Path('lego FOOTBALL MANIA/Setup/English/program files/LEGO Media/Soccer Mania/Game.exe'),Path('Lego Football Mania Dutch/Game.exe')]
a,b=[p.read_bytes() for p in paths]
p=pefile.PE(data=a); base=p.OPTIONAL_HEADER.ImageBase
print('ENTRY',hex(base+p.OPTIONAL_HEADER.AddressOfEntryPoint))
for ent in p.DIRECTORY_ENTRY_IMPORT:
 print(ent.dll.decode(),[(hex(i.address),i.name.decode() if i.name else i.ordinal) for i in ent.imports if i.name and any(x in i.name for x in [b'File',b'Drive',b'Volume',b'Time',b'Performance',b'Direct3D',b'Reg',b'CommandLine'])])
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
for s in p.sections:
 if s.Name.rstrip(b'\0')!=b'.text':continue
 start=s.PointerToRawData; end=start+s.SizeOfRawData
 changes=[i for i in range(start,end) if a[i]!=b[i]]
 groups=[]
 for i in changes:
  if not groups or i-groups[-1][-1]>16:groups.append([i])
  else:groups[-1].append(i)
 print('TEXT GROUPS',len(groups),'bytes',len(changes))
 for g in groups[:45]:
  lo,hi=g[0],g[-1]+1; va=base+p.get_rva_from_offset(lo)
  print('\nOFFSET',hex(lo),'VA',hex(va),'len',hi-lo,'old',a[lo:hi].hex(),'new',b[lo:hi].hex())
  for label,data in [('A',a),('B',b)]:
   print(label,'; '.join(f'{i.address:x} {i.mnemonic} {i.op_str}' for i in md.disasm(data[lo:hi+20],va)))
