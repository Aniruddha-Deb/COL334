#!/usr/bin/env python
# coding: utf-8

# In[13]:


import matplotlib.pyplot as plt
import pandas as pd


# In[14]:


data_files = {
    'NewReno': 'Q1/a3q1_NewReno.cwnd',
    'Vegas': 'Q1/a3q1_Vegas.cwnd',
    'Veno': 'Q1/a3q1_Veno.cwnd',
    'Westwood': 'Q1/a3q1_Westwood.cwnd',
}


# In[15]:


dfs = {f: pd.read_csv(p, delim_whitespace=True, header=None, names=['Time', 'StartWnd', 'EndWnd']) for f,p in data_files.items()}


# In[16]:


dfs['Vegas']


# In[17]:


for title,df in dfs.items():
    plt.cla()
    plt.plot(df['Time'], df['EndWnd'])
    plt.title(f"Congestion window size for TCP {title}")
    plt.xlabel("Time (s)")
    plt.ylabel("Cwnd")
    plt.savefig(f"Q1/cwnd_plot_{title}.pdf")


# In[18]:


print(f"Max window size:")
for title,df in dfs.items():
    print(f"{title:10} {df['EndWnd'].max()}")


# # Q2

# In[20]:


import os 
const_adr = {}
const_cdr = {}
for file in os.listdir('Q2'):
    if file.endswith('.cwnd'):
        ftypes = file.split('.')[0].split('_')
        if ftypes[2] == 'c4Mbps':
            const_cdr[ftypes[1][1:]] = f"Q2/{file}"
        else:
            const_adr[ftypes[2][1:]] = f"Q2/{file}"

const_adr_dfs = {f: pd.read_csv(p, delim_whitespace=True, header=None, names=['Time', 'StartWnd', 'EndWnd']) for f,p in const_adr.items()}
const_cdr_dfs = {f: pd.read_csv(p, delim_whitespace=True, header=None, names=['Time', 'StartWnd', 'EndWnd']) for f,p in const_cdr.items()}


# In[23]:


for title,df in const_adr_dfs.items():
    plt.cla()
    plt.plot(df['Time'], df['EndWnd'])
    print(f"ADR=5, CDR={title}: {df['EndWnd'][30:].mean()}")
    plt.title(f"cwnd size for Channel Data Rate {title} (ADR=5Mbps)")
    plt.xlabel("Time (s)")
    plt.ylabel("Cwnd")
    plt.savefig(f"Q2/const_adr_{title}.pdf")
    
for title,df in const_cdr_dfs.items():
    plt.cla()
    plt.plot(df['Time'], df['EndWnd'])
    print(f"CDR=4, ADR={title}: {df['EndWnd'][30:].mean()}")
    plt.title(f"cwnd size for Application Data Rate {title} (CDR=4Mbps)")
    plt.xlabel("Time (s)")
    plt.ylabel("Cwnd")
    plt.savefig(f"Q2/const_cdr_{title}.pdf")


# In[9]:


df = pd.read_csv('Q3/a3q3.cwnd', delim_whitespace=True, header=None, names=['Time', 'StartWnd', 'EndWnd'])
plt.cla()
plt.plot(df['Time'], df['EndWnd'])
plt.title(f"cwnd size for Node 0")
plt.xlabel("Time (s)")
plt.ylabel("Cwnd")
plt.savefig(f"Q3/tcp_cwnd.pdf")


# In[ ]:




