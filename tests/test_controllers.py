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
   self.assertEqual(reader.read(),{'panel_open':True,'gameplay_active':True,'revision':12})
   writer[:]=bytes(32)
   self.assertIsNone(reader.read())
  finally:
   reader.close()
   writer.close()
