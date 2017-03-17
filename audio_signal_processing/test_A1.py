import pickle
from A1Part1 import readAudio
from A1Part2 import minMaxAudio
from A1Part3 import hopSamples


def run_tests():
    test_file = pickle.load(open('testInputA1.pkl', 'rb'))
    test_cases = test_file['testCases']
    output_types = test_file['outputType']
    # test Part1
    print('Checking A1Part1')
    sample_array = readAudio(test_cases['A1-part-1'][0]['inputFile'])
    print('Running assertion(s) on A1Part1')
    assert (isinstance(sample_array, output_types['A1-part-1'][0]))
    # test Part2
    print('Checking A1Part2')
    min_and_max = minMaxAudio(test_cases['A1-part-2'][0]['inputFile'])
    print('Running assertion(s) on A1Part2')
    assert (isinstance(min_and_max, output_types['A1-part-2'][0]))
    # test Part3
    print('Checking A1Part3')
    for test_case in test_cases['A1-part-3']:
        sample_array = hopSamples(test_case['x'], test_case['M'])
        print('Running assertion(s) on A1Part3')
        assert (isinstance(sample_array, output_types['A1-part-3'][0]))
    print('All tests passed!')


if __name__ == '__main__':
    run_tests()
