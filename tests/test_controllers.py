import unittest
from opensoccermania.controllers import desired_keys, transitions

class ControllerTests(unittest.TestCase):
 def test_xbox_menu_and_gameplay_are_distinct(self):
  self.assertEqual(desired_keys({0},(0,0),'Menu',.2),{'Enter'})
  self.assertEqual(desired_keys({1},(0,0),'Menu',.2),{'Escape'})
  self.assertEqual(desired_keys({0},(0,0),'Match',.2),{'S'})
  self.assertEqual(desired_keys({1},(0,0),'Match',.2),{'D'})
 def test_deadzone_and_diagonal(self):
  self.assertEqual(desired_keys(set(),(.1,-.1),'Match',.2),set())
  self.assertEqual(desired_keys(set(),(.8,-.8),'Match',.2),{'Right','Up'})
 def test_focus_loss_releases_only_owned_keys(self):
  self.assertEqual(transitions({'S','Up'},set()),(set(),{'S','Up'}))


class SettingsGestureTests(unittest.TestCase):
 def test_start_releases_as_pause(self):
  from opensoccermania.controllers import SettingsChord
  chord=SettingsChord()
  self.assertEqual(chord.update({6}),(set(),set()))
  self.assertEqual(chord.update({6}),(set(),set()))
  self.assertEqual(chord.update(set()),(set(),{'Space'}))
  self.assertEqual(chord.update(set()),(set(),set()))
 def test_chord_orders_and_hold(self):
  from opensoccermania.controllers import SettingsChord
  for first in ({4},{6}):
   chord=SettingsChord()
   self.assertEqual(chord.update(first)[1],set())
   self.assertEqual(chord.update({4,6})[1],{'F1'})
   self.assertEqual(chord.update({4,6})[1],set())
   self.assertEqual(chord.update({6})[1],set())
   self.assertEqual(chord.update(set())[1],set())
   self.assertEqual(chord.update({4,6})[1],{'F1'})
 def test_chord_keeps_other_buttons(self):
  from opensoccermania.controllers import SettingsChord
  self.assertEqual(SettingsChord().update({0,4,6}),({0},{'F1'}))
 def test_native_mode_and_override(self):
  from opensoccermania.controllers import effective_mode
  self.assertEqual(effective_mode('Match',True,None),'Match')
  state={'gameplay_active':True,'panel_open':False}
  self.assertEqual(effective_mode('Menu',True,state),'Match')
  self.assertEqual(effective_mode('Menu',False,state),'Menu')
  state['panel_open']=True
  self.assertEqual(effective_mode('Match',True,state),'Menu')
 def test_mailbox_flags_and_f1(self):
  import os, struct
  from opensoccermania.controllers import InputMailbox
  box=InputMailbox(os.getpid())
  try:
   box.write({'F1'},controller_present=True)
   magic,seq,flags,reserved=struct.unpack('<4I',box.page[:16])
   self.assertEqual((magic,flags,reserved),(0x494d534f,5,0))
   self.assertEqual(seq%2,0)
   self.assertEqual(box.page[16+0x3b],128)
   box.write(set(),background_test=True)
   self.assertEqual(struct.unpack('<I',box.page[8:12])[0],3)
  finally:box.close()

 def test_ui_reader_waits_for_native_publisher(self):
  import os, mmap, struct
  from opensoccermania.controllers import UIStateMailbox
  reader=UIStateMailbox(str(os.getpid())+'-ui-test')
  self.assertIsNone(reader.read())
  writer=mmap.mmap(-1,32,tagname=reader.name)
  try:
   writer[:]=struct.pack('<8I',0x554d534f,1,3,12,0,0,0,0)
   self.assertEqual(reader.read(),{'panel_open':True,'gameplay_active':True,'movie_active':False,'revision':12})
   writer[:]=bytes(32)
   self.assertIsNone(reader.read())
  finally:
   reader.close()
   writer.close()


class NativePadTransportTests(unittest.TestCase):
 def test_gameplay_never_becomes_keyboard_input(self):
  from opensoccermania.controllers import overlay_keys
  for buttons,axes in [({0,1,2,3,9,10,11,14},(1.,-1.)),(set(),(-1.,1.))]:
   self.assertEqual(overlay_keys(buttons,axes,False,.2),set())
  self.assertEqual(overlay_keys({0},(1.,0.),False,.2,{'F1'}),{'F1'})
  self.assertEqual(overlay_keys({0,11},(0.,0.),True,.2),{'Enter','Up'})
 def test_stick_and_hat_use_one_axis_channel(self):
  from opensoccermania.controllers import pad_axes
  self.assertEqual(pad_axes({14},(1.,0.),.2),(32767,0))
  self.assertEqual(pad_axes({11,13},(0.,0.),.2),(-32767,-32767))
  self.assertEqual(pad_axes(set(),(.1,-.1),.2),(0,0))
 def test_four_devices_keep_separate_stable_slots(self):
  from opensoccermania.controllers import PadSlots
  slots=PadSlots()
  self.assertEqual(slots.update([91,17]),(17,91,None,None))
  self.assertEqual(slots.update([91,17,2,3]),(17,91,2,3))
  self.assertEqual(slots.update([91,2,3]),(None,91,2,3))
  self.assertEqual(slots.update([91,2,3,100,101]),(100,91,2,3))
 def test_native_page_preserves_each_pad_and_activity_serial(self):
  import os,struct
  from opensoccermania.controllers import PadMailbox,InputMailbox
  pads=PadMailbox(os.getpid());keys=InputMailbox(os.getpid())
  try:
   pads.write([(17,32767,0,1),(91,-32767,0,4),None,(3,0,-32767,2)])
   self.assertEqual(struct.unpack('<4I',pads.page[:16]),(0x504d534f,1,2,1))
   self.assertEqual(struct.unpack('<IIiiIIII',pads.page[32:64])[:5],(17,1,32767,0,1))
   self.assertEqual(struct.unpack('<IIiiIIII',pads.page[64:96])[:5],(91,1,-32767,0,4))
   self.assertEqual(pads.page[96:128],bytes(32))
   keys.write(set(),controller_present=True,controller_active=True,activity_serial=42)
   self.assertEqual(struct.unpack('<II',keys.page[8:16]),(13,42))
   pads.sequence=keys.sequence=0xfffffffe
   pads.write([],False);keys.write(set(),False)
   self.assertEqual(struct.unpack('<I',pads.page[8:12])[0],0)
   self.assertEqual(struct.unpack('<I',keys.page[4:8])[0],0)
  finally:pads.close();keys.close()
 def test_view_releases_for_movie_skip_but_chord_consumes_it(self):
  from opensoccermania.controllers import SettingsChord
  chord=SettingsChord()
  self.assertEqual(chord.update({4})[1],set())
  self.assertEqual(chord.update(set())[1],{'Escape'})
  self.assertEqual(chord.update(set())[1],set())
  self.assertEqual(chord.update({4,6})[1],{'F1'})
  self.assertEqual(chord.update({4})[1],set())
  self.assertEqual(chord.update(set())[1],set())
