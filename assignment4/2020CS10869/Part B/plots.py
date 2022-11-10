#!/usr/bin/env python
# coding: utf-8

# In[4]:


import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import os


# In[44]:


conv = pd.read_csv('data/convergence.csv')

plt.figure(figsize=(10,5), dpi=100)
plt.xscale('symlog')
plt.plot(conv['Time'], conv['100']+0.15-0.075, label="100 ms", marker='o')
plt.plot(conv['Time'], conv['1000']+0.1-0.075, label="1000 ms", marker='o')
plt.plot(conv['Time'], conv['20000']+0.05-0.075, label="20000 ms", marker='o')
plt.plot(conv['Time'], conv['80000']-0.075, label="80000 ms", marker='o')
plt.legend()
plt.title('Convergence v/s Time')
plt.xlabel('Time (s)')
plt.ylabel('Converged')
plt.ylim(-0.2,1.8)
plt.savefig('data/q2a_convergence_all.pdf', bbox_inches='tight')


# In[45]:


for i in ['100', '1000', '20000', '80000']:
    plt.figure(figsize=(10,5), dpi=100)
    plt.xscale('symlog')
    plt.plot(conv['Time'], conv[i], marker='o')
    plt.title(f'Convergence v/s Time for delay={i}ms')
    plt.xlabel('Time (s)')
    plt.ylabel('Converged')
    plt.ylim(-0.2,1.8)
    plt.savefig(f'data/q2a_convergence_{i}ms.pdf', bbox_inches='tight')


# In[48]:


reach = pd.read_csv('data/reachability.csv')
plt.figure(figsize=(10,5), dpi=100)
plt.plot(reach['Time'], reach['Reach'], marker='o')
plt.title(f'Reachability v/s Time from src to dst')
plt.xlabel('Time (s)')
plt.ylabel('Reachable')
plt.xticks([49,51,60,119,121,150], rotation=60)
plt.ylim(-0.2,1.8)
plt.savefig(f'data/q2b_reachable.pdf', bbox_inches='tight')


# In[49]:


reach_3 = pd.read_csv('data/reachability_3.csv')
plt.figure(figsize=(10,5), dpi=100)
plt.plot(reach_3['Time'], reach_3['Reach'], marker='o')
plt.title(f'Reachability v/s Time from src to dst')
plt.xlabel('Time (s)')
plt.ylabel('Reachable')
plt.xticks([119,121,140,200], rotation=60)
plt.ylim(-0.2,1.8)
plt.savefig(f'data/q2b_reachable_3.pdf', bbox_inches='tight')


# In[ ]:




