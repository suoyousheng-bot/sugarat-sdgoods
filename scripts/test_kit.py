"""Check standalone source installation, conflict preservation and resource integrity."""

import io
import json
from pathlib import Path
import shutil
import tarfile
import tempfile
import unittest

import kit


class KitTests(unittest.TestCase):
    """Use isolated projects; never download tools or open a serial port."""

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="sugarat-kit-test-")
        self.directory = Path(self.temp.name).resolve()
        self.addCleanup(self.temp.cleanup)

    def test_initialization_contains_board_and_transitive_theme_sources(self):
        project = self.directory / "badge"
        result = kit.initialize(kit.ROOT, project, "my_badge", ["sdgoods-ui-watch"])
        self.assertEqual(result["components"], ["sdgoods-pixel-characters", "sdgoods-ui-watch"])
        self.assertIn("project(my_badge)", (project / "CMakeLists.txt").read_text())
        for name in ("sdgoods_board", "lvgl", "sugarat_watch", "sugarat_pixel"):
            self.assertTrue((project / "components" / name / "CMakeLists.txt").is_file())
        self.assertIn("Hello my_badge!", (project / "main/apps/ui_helloapp.c").read_text())
        self.assertFalse((project / "build").exists())
        self.assertTrue((project / "NOTICE").is_file())

    def test_existing_project_and_unknown_selection_are_preserved(self):
        project = self.directory / "badge"
        project.mkdir()
        sentinel = project / "my-code.c"
        sentinel.write_text("owned application\n")
        with self.assertRaisesRegex(ValueError, "already exists"):
            kit.initialize(kit.ROOT, project, "my_badge", [])
        self.assertEqual(sentinel.read_text(), "owned application\n")
        other = self.directory / "other"
        with self.assertRaisesRegex(ValueError, "Unknown component"):
            kit.initialize(kit.ROOT, other, "my_badge", ["absent"])
        self.assertFalse(other.exists())

    def test_component_conflict_causes_no_other_component_writes(self):
        project = self.directory / "badge"
        kit.initialize(kit.ROOT, project, "my_badge", [])
        conflict = project / "components/sugarat_watch/README.md"
        conflict.parent.mkdir()
        conflict.write_text("custom watch implementation\n")
        with self.assertRaisesRegex(ValueError, "Customized file preserved"):
            kit.install(kit.ROOT, project, ["sdgoods-case-time"])
        self.assertEqual(conflict.read_text(), "custom watch implementation\n")
        self.assertFalse((project / "components/sugarat_pixel").exists())
        self.assertFalse((project / "components/sugarat_time").exists())

    def test_component_retry_reuses_sources_without_modifying_entry_point(self):
        project = self.directory / "badge"
        kit.initialize(kit.ROOT, project, "my_badge", [])
        entry = project / "main/apps/ui_helloapp.c"
        original = entry.read_bytes()
        first = kit.install(kit.ROOT, project, ["sdgoods-case-time"])
        second = kit.install(kit.ROOT, project, ["sdgoods-case-time"])
        self.assertTrue(first["writtenFiles"])
        self.assertEqual(second["writtenFiles"], [])
        self.assertEqual(entry.read_bytes(), original)

    def test_mixed_theme_and_symlink_destinations_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "one UI style"):
            kit.initialize(kit.ROOT, self.directory / "badge", "my_badge", ["sdgoods-ui-game", "sdgoods-case-time"])
        project = self.directory / "badge"
        kit.initialize(kit.ROOT, project, "my_badge", ["sdgoods-ui-game"])
        with self.assertRaisesRegex(ValueError, "other UI style"):
            kit.install(kit.ROOT, project, ["sdgoods-ui-watch"])
        alias = self.directory / "alias"
        try:
            alias.symlink_to(self.directory, target_is_directory=True)
        except OSError as error:
            self.skipTest(f"Host does not allow directory symlinks: {error}")
        with self.assertRaisesRegex(ValueError, "symlink"):
            kit.initialize(kit.ROOT, alias / "other", "my_badge", [])
        self.assertFalse((self.directory / "other").exists())

    def test_modified_resources_fail_checksum_verification(self):
        root = self.directory / "kit"
        shutil.copytree(kit.ROOT, root, ignore=shutil.ignore_patterns("__pycache__", ".git"))
        path = root / "boards/board_pins.h"
        path.write_bytes(path.read_bytes() + b"\n/* modified */\n")
        with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
            kit.verify(root)

    def test_unsafe_archive_is_rejected_even_with_matching_archive_digest(self):
        root = self.directory / "archive"
        root.mkdir()
        archive = root / "unsafe.tar.gz"
        with tarfile.open(archive, "w:gz") as stream:
            member = tarfile.TarInfo("../escape.c")
            member.size = 1
            stream.addfile(member, io.BytesIO(b"x"))
        record = {"archiveBytes": archive.stat().st_size, "archiveSha256": kit.digest(archive.read_bytes()),
                  "unpackedBytes": 1, "files": [{"path": "../escape.c", "bytes": 1, "sha256": kit.digest(b"x")}]}
        (root / "manifest.json").write_text(json.dumps(record))
        with self.assertRaisesRegex(ValueError, "Invalid resource path"):
            kit.template_files(root, {"template": {"manifest": "manifest.json", "archive": "unsafe.tar.gz"}})
        self.assertFalse((root.parent / "escape.c").exists())

    def test_play_lookup_and_prompt_pin_source_hardware_and_release(self):
        record = kit.select_play(kit.ROOT, "sugarat-radio")
        self.assertEqual(record["sourceDirectory"], "plays/sugarat-radio")
        self.assertEqual(record["productId"], "sdgoods-esp32s3-round")
        self.assertFalse(record["validation"]["deviceVerified"])
        output = kit.play_prompt(kit.ROOT, "sugarat-radio", "https://github.com/example/badge-kit", "v0.2.0", "Change the skin")
        for value in ("/blob/v0.2.0/AGENTS.md", "plays --id sugarat-radio", "plays/sugarat-radio", "Change the skin"):
            self.assertIn(value, output)
        with self.assertRaisesRegex(ValueError, "Unknown play"):
            kit.select_play(kit.ROOT, "renamed-radio")

    def test_play_files_and_hardware_must_match_the_release(self):
        root = self.directory / "kit"
        shutil.copytree(kit.ROOT, root, ignore=shutil.ignore_patterns("__pycache__", ".git"))
        path = root / "plays/sugarat-radio/main/main.c"
        path.write_bytes(path.read_bytes() + b"\n/* changed */\n")
        with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
            kit.verify(root)
        manifest = root / "plays/sugarat-radio/play.json"
        record = json.loads(manifest.read_text())
        record["productId"] = "other-board"
        manifest.write_text(json.dumps(record))
        with self.assertRaisesRegex(ValueError, "hardware mismatch"):
            kit.play_records(root, kit.json_file(root / "resources.lock.json"))
        with self.assertRaisesRegex(ValueError, "Invalid resource path"):
            lock = kit.json_file(root / "resources.lock.json")
            lock["plays"] = "../plays.json"
            kit.play_records(root, lock)

    def test_handoff_contains_matching_fixed_entry_and_one_line_request(self):
        output = kit.prompt("https://github.com/example/badge-kit.git", "v0.1.0", "番茄钟\n中文界面")
        self.assertIn("https://github.com/example/badge-kit/blob/v0.1.0/AGENTS.md", output)
        self.assertIn("番茄钟 中文界面", output)
        self.assertNotIn("\n", output)
        with self.assertRaisesRegex(ValueError, "GitHub repository URL"):
            kit.prompt("https://github.com/example/badge-kit/blob/main", "v0.1.0", "timer")


if __name__ == "__main__":
    unittest.main()
