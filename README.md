# Redbase

A simple DBMS.

### Framework

<br />![image.png](https://cdn.nlark.com/yuque/0/2020/png/1532622/1597657533761-53653339-e3bc-4a4b-9c0f-eff5ab562a5d.png#align=left&display=inline&height=203&margin=%5Bobject%20Object%5D&name=image.png&originHeight=636&originWidth=1026&size=86342&status=done&style=none&width=327)<br />


---

<a name="tretI"></a>
### Layers

<br />![image.png](https://cdn.nlark.com/yuque/0/2020/png/1532622/1597660192335-d931612f-b43c-4bb2-a4e0-e13e7acfc22e.png#align=left&display=inline&height=738&margin=%5Bobject%20Object%5D&name=image.png&originHeight=738&originWidth=1484&size=0&status=done&style=none&width=1484)<br />
<br />
<br />![image.png](https://cdn.nlark.com/yuque/0/2020/png/1532622/1597660194432-93ad0e7f-5f00-4647-bccb-570537f4459b.png#align=left&display=inline&height=778&margin=%5Bobject%20Object%5D&name=image.png&originHeight=778&originWidth=1285&size=0&status=done&style=none&width=1285)<br />
<br />
<br />![image.png](https://cdn.nlark.com/yuque/0/2020/png/1532622/1597660196159-4ac624e8-4cb8-4b4a-8143-fe5054c9fc72.png#align=left&display=inline&height=565&margin=%5Bobject%20Object%5D&name=image.png&originHeight=565&originWidth=1172&size=0&status=done&style=none&width=1172)<br />
<br />
<br />![image.png](https://cdn.nlark.com/yuque/0/2020/png/1532622/1597660198457-116e0cd3-954d-421c-99db-0c3ac9ea2801.png#align=left&display=inline&height=517&margin=%5Bobject%20Object%5D&name=image.png&originHeight=517&originWidth=1259&size=0&status=done&style=none&width=1259)<br />
<br />
<br />![image.png](https://cdn.nlark.com/yuque/0/2020/png/1532622/1597660199898-dfe96bfa-7c96-4977-88ac-d714a8dc1e0e.png#align=left&display=inline&height=1140&margin=%5Bobject%20Object%5D&name=image.png&originHeight=1140&originWidth=1639&size=0&status=done&style=none&width=1639)<br />
<br />
<br />


---



<a name="lM1Cj"></a>
### Syntax Tree

![image.png](https://cdn.nlark.com/yuque/0/2020/png/1532622/1597657976317-10e76c64-3df0-4f69-b6ee-91447baa00b8.png#align=left&display=inline&height=365&margin=%5Bobject%20Object%5D&name=image.png&originHeight=1086&originWidth=996&size=338441&status=done&style=none&width=335)

---

<a name="ygtJA"></a>
### Performance:

<br />


| DBMS | redbase | mysql |
| --- | --- | --- |
| Create table | create table tb (id i,name c10,age i,money f);<br />create index tb(id); | create table tb (id int primary_key,<br />                 name varchar(20),<br />                 age int,<br />                 money float); |
| Insert 10000 tuples | 0.5s | 2.5s |
| Select(select * from tb;)  10000 tuples  | 0.0145s | 0.017s |
| Select(select * from tb where id=xxx) 10000 times | 0.3s | 1s |
| Select(select * from tb where age=xxx) 10000 times | 11s | 36s |

