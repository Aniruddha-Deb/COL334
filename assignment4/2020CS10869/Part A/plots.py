#!/usr/bin/env python
# coding: utf-8

# In[4]:


import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import os


# In[50]:


data_files = [f for f in os.listdir('data') if f.endswith(".cwnd")]

dfs = {f: pd.read_csv(f"data/{f}", delim_whitespace=True, header=None, names=['Time', 'StartWnd', 'EndWnd']) for f in data_files}


# In[51]:


for fname,df in dfs.items():
    plt.figure(figsize=(20,5), dpi=100)
    plt.cla()
    plt.plot(df['Time'], df['EndWnd'])
    title_parts = fname.split('.')[0].split('_')
    title = f"{title_parts[1]} ({title_parts[2]})"    
    plt.title(f"Congestion window size for TCP {title}")
    plt.xlabel("Time (s)")
    plt.ylabel("Cwnd")
    plt.xlim(0,30)
    plt.savefig(f"data/cwnd_plot_{title_parts[1]}_{title_parts[2]}.pdf", bbox_inches="tight")
