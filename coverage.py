import json
import os
import sys

with open('config.json', encoding="utf8") as json_file:
    global config
    config = json.load(json_file)


def main():
    if sys.argv:
        filename = sys.argv[1]
        c_file = open(f"{filename}", "r+")
        reconstruct_inputs(c_file, sys.argv[2:])
        exec_commands(filename)
        print_json(filename)


def reconstruct_inputs(c_file, args):
    new_file = open(f"{c_file.name.replace('.c', '')}_inputs.c", "w+")

    flag = False
    arg_counter = 0
    for line in c_file.readlines():
        if line.startswith("int main"):
            flag = True
        elif ("int" in line or "float" in line or "double" in line or "char" in line) and flag:
            inst = line.split(" ")
            if not (inst.__contains__("for") or inst.__contains__("while")):
                for i in inst:
                    if not (i.startswith("int") or i.startswith("float") or i.startswith("double") or i.startswith(
                            "char")):
                        if i.endswith(","):
                            var_name = i.replace(",", "")
                            if var_name.isalnum() or var_name.isalpha():
                                new_line = f"{var_name} = {args[arg_counter]},"
                                arg_counter += 1
                                inst.insert(inst.index(i), new_line)
                                inst.remove(inst[inst.index(i)])
                            else:
                                new_line = f"{inst[inst.index(i) - 1]} = {args[arg_counter]},"
                                arg_counter += 1
                                inst.remove(inst[inst.index(i) - 1])
                                inst.insert(inst.index(i) - 1, new_line)
                        elif i.endswith(";\n"):
                            index = inst.index(i)
                            while not inst[index].isalpha():
                                inst.remove(inst[index])
                                index -= 1
                            new_line = f"{inst[index]} = {args[arg_counter]};\n"
                            arg_counter += 1
                            inst.remove(inst[index])
                            inst.insert(index, new_line)
                new_file.write(" ".join(inst))
                continue
        new_file.write(line)


def exec_commands(filename: str):
    filename_nonex = filename.replace(".c", "")
    # clang compilation
    os.system(config["commands"]["compile"].replace("foo.cc", filename).replace("foo", filename_nonex))
    # run instrumented program
    os.system(config["commands"]["run_inst_prog"].replace("foo", filename_nonex))
    # creating coverage report
    os.system(config["commands"]["cov_rep"].replace("foo", filename_nonex))
    # show coverage
    os.system(config["commands"]["show_cov"].replace("foo", filename_nonex))
    # compile gcov
    os.system(config["commands"]["gcov"].replace("foo", filename_nonex))
    # output
    os.system(
        config["commands"]["output"].replace("foo", filename_nonex).replace("output", f"{filename_nonex}_cov_res"))


def print_json(filename):
    filename_nonex = filename.replace(".c", "")
    with open(f"{filename_nonex}_cov_res.json", "r", encoding="utf8") as f:
        output_json = json.load(f)
        region_c = 1
        for region in output_json["data"][0]["functions"][0]["regions"]:
            print(f"Region {region_c}:")
            print(f"\tStart Line: {region[0]} \n\tStart Column: {region[1]}\n\tEnd Line: {region[2]}\n\tEnd Column: {region[3]}\n\tExecuted Count: {region[4]}")
            region_c += 1


main()
