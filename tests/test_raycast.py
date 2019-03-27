import numpy as np
import pytest

import terrain


def test_simple_join():
    hills = terrain.terrain(size=(30, 30), center=(0.0, 0.0), scale=0.1)
    hills.bump(0.08, 0.41, 8.48, 1.56, 'hill')
    hills.bump(0.77, 0.45, 10.46, 1.51, 'hill')
    hills.bump(0.52, 0.69, 10.56, 1.95, 'hill')
    hills.bump(0.66, 0.96, 9.25, 1.25, 'hill')
    hills.bump(0.45, 0.53, 11.69, 2.14, 'hill')
    hills.bump(0.30, 0.46, 9.05, 1.01, 'hill')
    hills.bump(0.68, 0.58, 11.71, 1.33, 'hill')
    hills.bump(0.58, 0.79, 8.16, 0.94, 'hill')
    hills.bump(0.99, 0.49, 9.58, 1.23, 'hill')
    hills.bump(0.10, 0.91, 11.35, 2.00, 'hill')
    vert, norm = np.array(hills.raycast([0.0, 0.0, 1.0], [0.4, 0.7, -1.0]))
    np.testing.assert_almost_equal(vert, [0.10705419, 0.18734482, 0.46472907], decimal=4)
    np.testing.assert_almost_equal(norm, [-0.0410928, -0.1731015, 0.9840464], decimal=4)


if __name__ == '__main__':
    pytest.main([__file__])
