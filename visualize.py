# FLASK_APP=visualize.py python3 -m flask

import matplotlib.pyplot as plt
import numpy as np
from collections import defaultdict

insn_summary = []
func_call_summary = []
line_count_summary = []
line_count_per_file_summary = []

func_names = set()
file_names = set()
line_names = set()

line_histogram = defaultdict(int)

for i in range(36096): #36096
    print(i)

    lines = open(f"blocks/insn_summary{i}", "r").readlines()
    lines = [line.strip().split(' ') for line in lines]
    lines = [(' '.join(line[:-1]), int(line[-1])) for line in lines]
    insn_summary.append(dict(lines))
    func_names.update(dict(lines).keys())

    lines = open(f"blocks/func_call_summary{i}", "r").readlines()
    lines = [line.strip().split(' ') for line in lines]
    lines = [(' '.join(line[:-1]), int(line[-1])) for line in lines]
    func_call_summary.append(dict(lines))

    lines = open(f"blocks/line_count_summary{i}", "r").readlines()
    lines = [line.strip().split(' ') for line in lines]
    lines = [(' '.join(line[:-1]), int(line[-1])) for line in lines]
    line_count_summary.append(dict(lines))
    line_names.update(dict(lines).keys())
    tmp = dict(lines)

    for line_name in tmp:
        line_histogram[line_name] += tmp[line_name]

    lines = open(f"blocks/line_count_per_file_summary{i}", "r").readlines()
    lines = [line.strip().split(' ') for line in lines]
    lines = [(' '.join(line[:-1]), int(line[-1])) for line in lines]
    line_count_per_file_summary.append(dict(lines))
    file_names.update(dict(lines).keys())

print(line_histogram)

from flask import Flask
from flask import jsonify
from flask import request

app = Flask(__name__)

@app.route("/func_names")
def return_func_names():
    response = jsonify(results=list(func_names))
    response.headers.add('Access-Control-Allow-Origin', '*')
    return response

@app.route("/file_names")
def return_file_names():
    response = jsonify(results=list(file_names))
    response.headers.add('Access-Control-Allow-Origin', '*')
    return response

@app.route("/line_names")
def return_line_names():
    response = jsonify(results=list(line_names))
    response.headers.add('Access-Control-Allow-Origin', '*')
    return response

@app.route("/data_function_instructions")
def return_data_function_instructions():
    selector = request.args.get('selector')
    result = []
    for e in insn_summary:
        if selector in e:
            result.append(e[selector])
        else:
            result.append(0)
    response = jsonify(results=list(result))
    response.headers.add('Access-Control-Allow-Origin', '*')
    return response

@app.route("/data_function_calls")
def return_data_function_calls():
    selector = request.args.get('selector')
    result = []
    for e in func_call_summary:
        if selector in e:
            result.append(e[selector])
        else:
            result.append(0)
    response = jsonify(results=list(result))
    response.headers.add('Access-Control-Allow-Origin', '*')
    return response

@app.route("/data_files")
def return_data_files():
    selector = request.args.get('selector')
    result = []
    for e in line_count_per_file_summary:
        if selector in e:
            result.append(e[selector])
        else:
            result.append(0)
    response = jsonify(results=list(result))
    response.headers.add('Access-Control-Allow-Origin', '*')
    return response

@app.route("/data_lines")
def return_data_lines():
    selector = request.args.get('selector')
    result = []
    for e in line_count_summary:
        if selector in e:
            result.append(e[selector])
        else:
            result.append(0)
    response = jsonify(results=list(result))
    response.headers.add('Access-Control-Allow-Origin', '*')
    return response

@app.route("/list_file")
def return_list_file():
    selector = request.args.get('selector')
    lines = open(selector, 'r').readlines()
    lines = map(lambda l: l.strip(), lines)
    lines = list(enumerate(lines))
    lines = [(line_histogram[f'{selector}:{lno + 1}'], lcon) for (lno, lcon) in lines]
    response = jsonify(results=list(lines))
    response.headers.add('Access-Control-Allow-Origin', '*')
    return response

# import sys

# for line in sys.stdin:
#     line = line.rstrip()
#     cnts = []
#     for cnt in all_lines:
#         if line in cnt:
#             cnts.append(cnt[line])
#         else:
#             cnts.append(0)
#     print(cnts)
#     fig = plt.figure()
#     ax = plt.axes()
#     ax.plot([x * 50.0 / 1000.0 for x in range(18048)], cnts)
#     plt.show()


#plt.ion()

#line = plt.scatter(x = [1,100], y = [1,100], marker='s')
#for i in range(100):
#    line.set_offsets([[1,1],[100,100 + 5 * i]])
#    plt.draw()
#    plt.pause(1)
