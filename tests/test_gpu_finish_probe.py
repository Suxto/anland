"""Source-order guard only; the native CI build and device probe are separate."""
from pathlib import Path
import unittest


class GPUFinishProbeTest(unittest.TestCase):
    def test_wait_precedes_damage_accounting_and_fence_handoff(self):
        root = Path(__file__).resolve().parents[1]
        source = (root / 'producers/kde/anland_backend/src/backends/anland/anland_egl_backend.cpp').read_text()
        body = source.split('bool AnlandEglLayer::endFrame(', 1)[1].split('\nvoid AnlandEglLayer::setPendingFence', 1)[0]
        self.assertIn('    glFinish();\n    // endFrame is owned', body)
        self.assertNotIn('glFlush();', body)
        self.assertLess(body.index('    glFinish();'), body.index('for (int i = 0;'))
        self.assertLess(body.index('    glFinish();'), body.index('EGLNativeFence fence'))
        self.assertLess(body.index('EGLNativeFence fence'), body.index('setPendingFence('))
        self.assertEqual(body.count('ANLAND_GPU_FINISH_PROBE active:'), 1)
        self.assertIn('if (!probeReported)', body)


if __name__ == '__main__':
    unittest.main()
