import unittest

# Import test modules
import errands.tests.widgets.test_window


def load_tests():
    tests: list[unittest.TestSuite] = []
    loader = unittest.TestLoader()
    tests.append(loader.loadTestsFromModule(errands.tests.widgets.test_window))
    return tests


def run_tests():
    unittest.TextTestRunner().run(unittest.TestSuite(load_tests()))
