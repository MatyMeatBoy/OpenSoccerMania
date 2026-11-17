"""Asset-free ISO fixture for checking a portable import without starting a game."""
def write_game_iso(path):
    image=bytearray(32*2048)
    def dual(value,width):return value.to_bytes(width,'little')+value.to_bytes(width,'big')
    def record(name,extent,size,directory=False):
        raw=bytearray(33+len(name)+(len(name)%2==0));raw[0]=len(raw)
        raw[2:10]=dual(extent,4);raw[10:18]=dual(size,4)
        raw[18:25]=bytes((126,1,1,0,0,0,0));raw[25]=2 if directory else 0
        raw[28:32]=dual(1,2);raw[32]=len(name);raw[33:33+len(name)]=name
        return raw
    exe=b'MZOpenSoccerMania synthetic import fixture - never execute'.ljust(512,b'\0')
    text=b'HELLO: Synthetic import check\n'
    pvd=bytearray(2048);pvd[:7]=b'\x01CD001\x01';pvd[80:88]=dual(32,4)
    pvd[120:124]=dual(1,2);pvd[124:128]=dual(1,2);pvd[128:132]=dual(2048,2)
    pvd[156:190]=record(b'\x00',20,2048,True);pvd[881]=1
    image[16*2048:17*2048]=pvd;image[17*2048:17*2048+7]=b'\xffCD001\x01'
    entries={20:(20,[(b'MANIA',21,2048,True),(b'GAME.EXE;1',26,len(exe),False)]),
             21:(20,[(b'CD',22,2048,True)]),22:(21,[(b'MESSAGES',23,2048,True),(b'SOUND',24,2048,True)]),
             23:(22,[(b'ENGLISH.TXT;1',27,len(text),False)]),24:(22,[])}
    for sector,(parent,children) in entries.items():
        raw=record(b'\x00',sector,2048,True)+record(b'\x01',parent,2048,True)
        for child in children:raw+=record(*child)
        image[sector*2048:sector*2048+len(raw)]=raw
    image[26*2048:26*2048+len(exe)]=exe;image[27*2048:27*2048+len(text)]=text
    path.write_bytes(image)
