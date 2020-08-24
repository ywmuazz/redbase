# RQL

## System Command

```dbcreate DBname```

Create a database.

```dbdestroy DBname```

Destroy a database.

```redbase DBname```

Open a database.

```create table relName(attrName1 Type1, attrName2 Type2, ..., attrNameN TypeN);```

The create table command creates a relation with the specified name and schema. 

```drop table relName;```

The drop table command destroys relation relName, along with all indexes on the relation.

```create index relName(attrName);```

The create index command creates an index on attribute attrName of relation relName and builds the index for the current tuples in the relation. Only one index may be created for each attribute of a relation.

```drop index relName(attrName);```

The drop index command destroys the index on attribute attrName of relation relName.


```load relName("FileName");```

The load utility performs bulk loading of the named relation from the specified Unix file: all tuples specified in the load file are inserted into the relation. 

```help;``` or ```help relName;```

If relName is not specified, then the help utility prints the names of all relations in the database. 

```print relName;```

The print utility displays the current set of tuples in relation relName.

```set Param = "Value";```

The set utility allows the user to set system parameters without needing to recompile the system, or even exit the RedBase prompt. 

---
## Query Language


- Select Command
```
Select 
    A1, A2, …, Am
From 
    R1, R2, …, Rn
[
Where 
    A1’ comp1 AV1 And A2’ comp2 AV2 And … And Ak’ compk AVk
];
```

This command has the standard SQL interpretation: The result of the command is structured as a relation with attributes A1, A2, …, Am. For each tuple t in the cross-product of relations R1, R2, …, Rn in the From clause, if tuple t satisfies all conditions in the Where clause, then the attributes of tuple t listed in the Select clause are included in the result (in the listed order). If the Where clause is omitted, it is considered to be true always. Duplicate tuples in the result are not eliminated.

- Insert Command 
```
Insert Into relName Values (V1, V2, …, Vn);
```
This command inserts into relation relName a new tuple with the specified attribute values. As in bulk loading, attribute values must appear in the same order as in the create table command that was executed for relation relName. Values are specified in the same way as in load files and the Select command. String values can be of any length up to the length specified for the corresponding attribute.


- Delete Command 
```
Delete From relName
[
Where 
    A1 comp1 AV1 And A2 comp2 AV2 And … And Ak compk AVk
];
```
This command deletes from relation relName all tuples satisfying the Where clause. As in the Select command, the conditions in the Where clause compare attributes to constant values or to other attributes. In the Where clause, all attributes may be specified simply as attrName (since there is only one relName), although specifying the relName is not an error. Comparison operators and specification of values are the same as in the Select command. If the Where clause is omitted, all tuples in relName are deleted.

- Update Command

```
Update 
    relName
Set 
    attrName = AV
[
Where 
    A1 comp1 AV1 And A2 comp2 AV2 And … And Ak compk AVk
];
```

This command updates in relation relName all tuples satisfying the Where clause. The Where clause is exactly as in the Delete command described above. AV on the right-hand side of the equality operator is either an attribute or a constant value. An attribute may be specified simply as attrName (since there is only one relName), although specifying the relName is not an error. A value is specified as in the Select command. Each updated tuple is modified so that its attribute attrName gets the value AV (if AV is a constant), or the value of the updated tuple's AV attribute (if AV is an attribute). If the Where clause is omitted, all tuples in relName are updated.