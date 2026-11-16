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
