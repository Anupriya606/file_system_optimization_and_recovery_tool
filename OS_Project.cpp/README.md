# Simple File System Simulation (C++)

This project is a **C++-based simulation of a simple file system**, designed to help students understand how operating systems internally manage storage, track free space, and recover deleted data.

## 🚀 Overview

The system creates a **virtual disk** divided into fixed-size blocks and uses a **bitmap** to keep track of free and allocated blocks. It supports essential file system functionalities such as:

* **File creation**
* **File deletion**
* **File recovery** (if original blocks are still available)
* **Viewing free space**
* **Listing existing and deleted files**

A **menu-driven console interface** makes it easy for users to interact with the system.

## 🧠 Key Concepts Demonstrated

This project is an educational model illustrating fundamental Operating System concepts:

### 📦 Block Allocation

The virtual disk is split into equal-sized blocks. Each file is stored across one or more of these blocks depending on its size.

### 🗂️ Free-Space Management

A bitmap structure is used to efficiently track which blocks are **free** or **allocated**, similar to real OS file systems.

### 🔄 Data Recovery

Deleted files are not immediately lost. Their metadata is preserved, and files can be **restored** if their original blocks have not been overwritten.

### 🧩 Modular System Design

The project uses a modular approach to separate:

* Disk management
* Metadata handling
* File operations
* User interface logic

## 📚 Features

* Virtual disk created using a fixed-size image file
* Bitmap-managed block allocation
* Metadata storage for active and deleted files
* Recovery of deleted files (when possible)
* Clear and interactive console menu

## 🖥️ How to Run

1. Compile using a C++ compiler (g++ recommended):

   ```bash
   g++ -o filesystem main.cpp
   ```
2. Run the program:

   ```bash
   ./filesystem
   ```

## 🎯 Purpose

This simulation serves as a hands-on learning tool to deepen the understanding of:

* How file systems operate at the block level
* Techniques used by OS to maintain data integrity
* Internal mechanics behind file deletion and recovery
* Organizing software using modular principles

---

Feel free to **fork**, enhance, or contribute to this project! 🎉
