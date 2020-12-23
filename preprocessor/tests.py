import unittest
from preprocess import interpolate_exp

class TestInterpolator(unittest.TestCase):

    def test_interpolate(self):
        i = interpolate_exp(2.0,(10,0),(14,10))
        self.assertEqual(i(10),0)
        self.assertEqual(i(14),10)
        self.assertEqual(i(12),2.5)

if __name__ == '__main__':
    unittest.main()