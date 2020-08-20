# Redbase

A simple DBMS.

### Framework


<img src="https://cdn.nlark.com/yuque/0/2020/png/1532622/1597657533761-53653339-e3bc-4a4b-9c0f-eff5ab562a5d.png#align=left&display=inline&height=203&margin=%5Bobject%20Object%5D&name=image.png&originHeight=636&originWidth=1026&size=86342&status=done&style=none&width=327" width="500" >


---

### Layers



![image.png](https://raw.githubusercontent.com/ywmuazz/redbase/master/pic/pfxmind.png)
![image.png](https://raw.githubusercontent.com/ywmuazz/redbase/master/pic/rmxmind.png)
![image.png](https://raw.githubusercontent.com/ywmuazz/redbase/master/pic/ixxmind.png)
![image.png](https://raw.githubusercontent.com/ywmuazz/redbase/master/pic/smxmind.png)
![image.png](https://raw.githubusercontent.com/ywmuazz/redbase/master/pic/qlxmind.png)


---



### Syntax Tree

sample:
```sql
create table tb1 (id i,name c10,score i);
create table tb2 (id i,age i);
create table tb3 (id i,money float);

select 
	id 
from 
	tb1,tb2,tb3 
where 
	tb1.name="Mike" and tb1.score=99 and tb1.id=tb2.id and tb2.id=tb3.id and tb2.age=18 and tb3.money=100.0;
```

<img src="https://cdn.nlark.com/yuque/0/2020/png/1532622/1597921322678-e57e1a67-e6b3-47e1-907a-c28773394f48.png#align=left&display=inline&height=1383&margin=%5Bobject%20Object%5D&name=image.png&originHeight=1383&originWidth=1233&size=532909&status=done&style=none&width=1233" width="500" >


---

### Performance:

<br />


| DBMS | redbase | mysql |
| --- | --- | --- |
| Create table | create table tb (id i,name c10,age i,money f);<br />create index tb(id); | create table tb (id int primary_key,<br />                 name varchar(20),<br />                 age int,<br />                 money float); |
| Insert 10000 tuples | 0.5s | 2.5s |
| Select(select * from tb;)  10000 tuples  | 0.0145s | 0.017s |
| Select(select * from tb where id=xxx) 10000 times | 0.3s | 1s |
| Select(select * from tb where age=xxx) 10000 times | 11s | 36s |

