import unittest


class ControllerPromptTests(unittest.TestCase):
    def test_sdl_family_is_authoritative(self):
        from opensoccermania.controller_prompts import ControllerFamily as F, detect_family
        for device_type in (2, 3):
            self.assertEqual(detect_family(device_type, 'DualShock virtual XInput'), F.XBOX360)
        for device_type in (4, 5, 6):
            self.assertEqual(detect_family(device_type, 'Wireless Controller'), F.PS4)
        self.assertEqual(detect_family(12, 'Steam Controller'), F.STEAM_DECK)
        for device_type in (7, 8, 9, 10):
            self.assertEqual(detect_family(device_type, 'Pro Controller'), F.SWITCH_PRO)

    def test_raw_names_and_unknown_fallback(self):
        from opensoccermania.controller_prompts import ControllerFamily as F, detect_family
        for name in ('DualShock 4', 'Sony Wireless Controller', 'PS4 Controller', 'DualSense'):
            self.assertEqual(detect_family(0, name), F.PS4)
        for name in ('Nintendo Switch Pro Controller', 'Switch Pro', 'Nintendo Joy-Con Pair'):
            self.assertEqual(detect_family(0, name), F.SWITCH_PRO)
        self.assertEqual(detect_family(0, 'Generic USB Gamepad'), F.XBOX360)
        self.assertEqual(detect_family(999, ''), F.XBOX360)
        self.assertEqual(detect_family(0, 'Steam Deck Controller'), F.STEAM_DECK)

    def test_face_labels_follow_positions_without_swapping_actions(self):
        from opensoccermania.controller_prompts import ControllerFamily as F, face_labels
        self.assertEqual(face_labels(F.XBOX360), ('A', 'B', 'X', 'Y'))
        self.assertEqual(face_labels(F.PS4), ('Cross', 'Circle', 'Square', 'Triangle'))
        self.assertEqual(face_labels(F.SWITCH_PRO), ('B', 'A', 'Y', 'X'))
        self.assertEqual(face_labels(F.STEAM_DECK), ('A', 'B', 'X', 'Y'))
        from opensoccermania.controllers import desired_keys
        self.assertEqual(desired_keys({0}, (0, 0), 'Menu', .2), {'Enter'})
        self.assertEqual(desired_keys({1}, (0, 0), 'Menu', .2), {'Escape'})

    def test_manual_style_validation_and_auto(self):
        from opensoccermania.controller_prompts import ControllerFamily as F, resolve_family
        self.assertEqual(resolve_family('Auto', F.PS4), F.PS4)
        self.assertEqual(resolve_family('Xbox 360', F.PS4), F.XBOX360)
        self.assertEqual(resolve_family('PlayStation 4', F.XBOX360), F.PS4)
        self.assertEqual(resolve_family('Switch Pro', F.PS4), F.SWITCH_PRO)
        self.assertEqual(resolve_family('invalid', F.SWITCH_PRO), F.SWITCH_PRO)
        self.assertEqual(resolve_family('Auto', 999), F.XBOX360)
        self.assertEqual(resolve_family('Steam Deck', F.PS4), F.STEAM_DECK)

    def test_mailbox_family_bits_do_not_overlap_existing_flags(self):
        from opensoccermania.controller_prompts import ControllerFamily as F, family_flags
        self.assertEqual(family_flags(F.XBOX360), 0x100)
        self.assertEqual(family_flags(F.PS4), 0x200)
        self.assertEqual(family_flags(F.SWITCH_PRO), 0x300)
        self.assertEqual(family_flags(F.STEAM_DECK), 0x400)
        for value in (-1, 0, 5, 255):
            self.assertEqual(family_flags(value), 0)
        for family in F:
            self.assertFalse(family_flags(family) & 0x1f)

    def test_glyph_assets_use_canonical_positions(self):
        from opensoccermania.controller_prompts import ControllerFamily as F, asset_path
        self.assertEqual(asset_path(F.PS4, 'south'), 'controllers/ps4/south.png')
        self.assertEqual(asset_path(F.SWITCH_PRO, 'east'), 'controllers/switch_pro/east.png')
        with self.assertRaises(ValueError):
            asset_path(F.XBOX360, '../outside')


class ControllerAssetTests(unittest.TestCase):
    def test_native_atlas_contains_all_family_faces_and_preserves_padding(self):
        from pathlib import Path
        import struct
        from tools.build_controller_prompts import build_atlas, validate_atlas
        root = Path(__file__).resolve().parents[1]
        data = build_atlas(root)
        validate_atlas(data)
        self.assertEqual(len(data), 32 + 4 * 4 * 64 * 64 * 4)
        images = [data[32+i*16384:32+(i+1)*16384] for i in range(16)]
        self.assertEqual(len(set(images)), 16)
        for image in images:
            self.assertEqual(image[:4], bytes(4))
            self.assertGreater(sum(bool(image[i]) for i in range(3, len(image), 4)), 1000)
        broken = bytearray(data); broken[-100] ^= 1
        with self.assertRaises(ValueError):
            validate_atlas(broken)
        with self.assertRaises(ValueError):
            validate_atlas(data[:-4])

    def test_upstream_png_hashes_and_license_are_present(self):
        from pathlib import Path
        import json, hashlib
        root = Path(__file__).resolve().parents[1]
        manifest = json.loads((root/'assets/controllers/manifest.json').read_text())
        self.assertEqual(manifest['license'], 'CC0-1.0')
        self.assertEqual(len(manifest['files']), 52)
        for record in manifest['files']:
            self.assertEqual(hashlib.sha256((root/record['path']).read_bytes()).hexdigest(), record['sha256'])
        self.assertIn('Creative Commons 0', (root/'licenses/Xelu-Controller-Prompts.txt').read_text(encoding='utf-8'))


class ControllerFamilyMailboxTests(unittest.TestCase):
    def test_metadata_preserves_page_size_keys_and_existing_flags(self):
        import io, struct
        from opensoccermania.controllers import InputMailbox
        box = object.__new__(InputMailbox)
        box.page = io.BytesIO(bytes(272)); box.sequence = 0
        box.write(set(), background_test=True, controller_present=True,
                  controller_active=True, activity_serial=123, controller_family=2)
        data = box.page.getvalue()
        self.assertEqual(len(data), 272)
        magic, sequence, flags, activity = struct.unpack_from('<4I', data)
        self.assertEqual((magic, sequence, activity), (0x494d534f, 2, 123))
        self.assertEqual(flags, 0x200 | 1 | 2 | 4 | 8)
        self.assertEqual(data[16:], bytes(256))
        box.write(set(), enabled=False, controller_present=False, controller_family=3)
        self.assertEqual(struct.unpack_from('<I', box.page.getvalue(), 8)[0], 0)

    def test_mixed_devices_use_selection_then_last_activity_without_stale_ids(self):
        from opensoccermania.controller_prompts import ControllerFamily as F, active_family
        devices = {11: F.XBOX360, 22: F.PS4, 33: F.SWITCH_PRO}
        self.assertEqual(active_family(devices, recent=22), F.PS4)
        self.assertEqual(active_family(devices, selected=33, recent=22), F.SWITCH_PRO)
        del devices[33]
        self.assertEqual(active_family(devices, selected=33, recent=22), F.PS4)
        del devices[22]
        self.assertEqual(active_family(devices, selected=33, recent=22), F.XBOX360)
        self.assertEqual(active_family({}), F.XBOX360)


class ControllerPromptPreferenceTests(unittest.TestCase):
    def test_profile_round_trip_and_invalid_live_values(self):
        from pathlib import Path
        import tempfile, configparser
        from opensoccermania.video import VideoSettings
        from opensoccermania.runtime import configure_bridge, read_live_settings
        for index, style in enumerate(('Auto', 'Xbox 360', 'PlayStation 4', 'Switch Pro', 'Steam Deck')):
            with tempfile.TemporaryDirectory() as temporary:
                entry = Path(temporary)
                settings = VideoSettings(controller_prompt_style=style, xbox_prompts=False, title_extension=False)
                configure_bridge(entry, settings)
                config = configparser.ConfigParser(); config.read(entry/'opensoccermania.ini')
                self.assertEqual(config.getint('Graphics', 'ControllerPromptFamily'), index)
                self.assertEqual(config.getint('Graphics', 'XboxPrompts'), 0)
                config['Graphics']['ControllerPromptFamily'] = str((index+1) % 5)
                with (entry/'opensoccermania.ini').open('w') as stream:
                    config.write(stream)
                changed = read_live_settings(entry, settings)
                self.assertEqual(changed.controller_prompt_style, ('Auto', 'Xbox 360', 'PlayStation 4', 'Switch Pro', 'Steam Deck')[(index+1) % 5])
                self.assertFalse(changed.xbox_prompts)
                config['Graphics']['ControllerPromptFamily'] = '99'
                with (entry/'opensoccermania.ini').open('w') as stream:
                    config.write(stream)
                self.assertEqual(read_live_settings(entry, settings).controller_prompt_style, style)
        with self.assertRaises(ValueError):
            VideoSettings(controller_prompt_style='invalid')


class ControllerStyleWriteTests(unittest.TestCase):
    def test_release_allows_only_declared_controller_assets(self):
        from tools.package_release import _validate_member
        from tools.package_portable import _member
        from opensoccermania.controller_prompts import BUTTON_ASSETS
        for family in ('xbox360', 'ps4', 'switch_pro', 'steam_deck'):
            _member('assets/controllers/'+family)
            for button in BUTTON_ASSETS:
                name=f'assets/controllers/{family}/{button}.png'
                _validate_member(name,'100644');_member(name)
        for name in ('assets/controllers/manifest.json','assets/controllers/controller_prompts.bin'):
            _validate_member(name,'100644');_member(name)
        for name in ('assets/controllers/game.bin','assets/controllers/ps4/unknown.png','assets/controllers/new-family/south.png'):
            with self.assertRaises(ValueError):_validate_member(name,'100644')
            with self.assertRaises(ValueError):_member(name)

    def test_style_only_update_preserves_other_settings(self):
        from pathlib import Path
        import tempfile, configparser
        from opensoccermania.runtime import write_live_prompt_style
        with tempfile.TemporaryDirectory() as temporary:
            entry = Path(temporary)
            path = entry/'opensoccermania.ini'
            path.write_text('[Graphics]\nRenderFPS=360\nXboxPrompts=0\n[Diagnostics]\nIsolateInstance=1\n')
            write_live_prompt_style(entry, 'Steam Deck')
            config = configparser.ConfigParser(); config.read(path)
            self.assertEqual(config.getint('Graphics', 'ControllerPromptFamily'), 4)
            self.assertEqual(config.getint('Graphics', 'RenderFPS'), 360)
            self.assertEqual(config.getint('Graphics', 'XboxPrompts'), 0)
            self.assertEqual(config.getint('Diagnostics', 'IsolateInstance'), 1)
            before = path.read_bytes()
            with self.assertRaises(ValueError):
                write_live_prompt_style(entry, 'invalid')
            self.assertEqual(path.read_bytes(), before)
