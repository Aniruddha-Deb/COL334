import sys
import subprocess
import re

def get_ping_time(ip):
    res = subprocess.run(["ping", "-c", "1", "-W", "2", ip], capture_output=True, text=True)

    res_lines = res.stdout.strip().split('\n')
    if (len(res_lines) > 4):
        return f"{res_lines[-1].split('/')[-3]} ms"
    else:
        return '*'

if __name__ == "__main__":
    host = sys.argv[1]

    for i in range(1,30):
        res = subprocess.run(["ping","-c","1","-m",str(2*i-1),"-W","2",host], capture_output=True, text=True)

        res_lines = res.stdout.strip().split('\n')
        # print(res_lines)
        if (res_lines[1].endswith('Time to live exceeded')):
            toks = res_lines[1].strip().split(' ')
            if '(' in res_lines[1]:
                # parse domain name and IP
                dom_name = toks[3]
                ip = toks[4][1:-2]
                print(f"{i}  {dom_name} ({ip}) {get_ping_time(ip)} {get_ping_time(ip)} {get_ping_time(ip)}")
            else: 
                dst_ip = res_lines[1].strip().split(' ')[3][:-1]
                print(f"{i}  {dst_ip} {get_ping_time(dst_ip)} {get_ping_time(dst_ip)} {get_ping_time(dst_ip)}")
        elif (res_lines[1] == ""):
            if (len(res_lines) <= 4):
                print(f"{i}  * * *")
            else:
                print(f"{i}  {host} {get_ping_time(host)} {get_ping_time(host)} {get_ping_time(host)}")
                break

