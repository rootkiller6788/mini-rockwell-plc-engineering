import os, sys

def w(path, content):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path,'w') as f: f.write(content)
    print(f"OK {path} ({len(content.splitlines())} lines)")

# ============== AOI source ==============
aoi_c = open('src_aoi_template.txt','r').read() if os.path.exists('src_aoi_template.txt') else ""
print("Template check done, len=", len(aoi_c))
