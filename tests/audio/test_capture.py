"""Native microphone lifecycle tests, exclusively fake AAudio/private sockets."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / 'tests/audio'


class AudioCaptureTests(unittest.TestCase):
    def test_parallel_app_identity_and_components(self):
        app = ROOT / 'consumers/anland_v5/android_consumer/app'
        gradle = (app / 'build.gradle').read_text()
        self.assertIn('applicationId "com.anland.consumer.micondemand"', gradle)
        self.assertIn("namespace 'com.anland.consumer'", gradle)  # retain JNI/class names
        android = '{http://schemas.android.com/apk/res/android}'
        manifest = ET.parse(app / 'src/main/AndroidManifest.xml').getroot()
        application = manifest.find('application')
        assert application is not None
        self.assertEqual(application.get(android + 'label'), 'Anland Mic Test')
        intent = ET.parse(app / 'src/main/res/xml/shortcuts.xml').find('shortcut/intent')
        assert intent is not None
        self.assertEqual(intent.get(android + 'targetPackage'), 'com.anland.consumer.micondemand')
        self.assertEqual(intent.get(android + 'targetClass'), 'com.anland.consumer.SettingsActivity')
        interceptor = (app / 'src/main/java/com/anland/consumer/KeyInterceptor.java').read_text()
        self.assertIn('new ComponentName(ctx, KeyInterceptor.class).flattenToShortString()', interceptor)
        self.assertNotIn('"com.anland.consumer/.KeyInterceptor"', interceptor)

    def compile_run(self, filename, extra):
        with tempfile.TemporaryDirectory() as folder:
            binary = Path(folder) / 'test'
            subprocess.run([
                'cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
                '-I' + str(TESTS / 'mocks'), '-I' + str(ROOT / 'libdisplay_consumer'),
                '-I' + str(ROOT / 'common'), str(TESTS / filename),
                '-o', str(binary), '-pthread', *extra,
            ], check=True, timeout=60)
            subprocess.run([str(binary)], check=True, timeout=20)

    @unittest.skipUnless(sys.platform == 'linux' and shutil.which('cc'), 'Linux native compiler required')
    def test_android_capture_with_fake_aaudio(self):
        self.compile_run('native_capture_test.c', [])

    @unittest.skipUnless(sys.platform == 'linux' and shutil.which('pkg-config'), 'Linux PipeWire headers required')
    def test_producer_state_and_capture_lease(self):
        flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'libpipewire-0.3'], text=True).split()
        self.compile_run('producer_capture_test.c', flags)


if __name__ == '__main__':
    unittest.main()
