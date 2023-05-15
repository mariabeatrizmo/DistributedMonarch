import subprocess
#import portpicker
import sys
import re

result       = subprocess.run(['ip', 'add'], capture_output=True, text=True).stdout
infiniband    = result[result.find('link/infiniband'):]
inet          = infiniband[infiniband.find('inet'):]
ip           = re.search(r'\d\d?\d?\.\d\d?\d?\.\d\d?\d?\.\d\d?\d?', inet)

if not ip:  
    result       = subprocess.run(['hostname', '-I'], capture_output=True, text=True).stdout
    ip           = re.search(r'\d\d?\d?\.\d\d?\d?\.\d\d?\d?\.\d\d?\d?', result)
    
aux = ip.group(0)
#ip_with_port = aux + ":" + str(portpicker.pick_unused_port())
#ip_with_port = aux + ":3333"
ip_with_port = aux

print(ip_with_port)