import tkinter as tk
import subprocess
import os

def handler(event, points, n):
	points.append((event.x, event.y))
	event.widget.config(text = str(n-len(points)))
	if len(points) == n:
		event.widget.update()
		event.widget.quit()

def resizer(points, size):
	CONST = 32767
	new_points = []
	width, height = size.split("x")
	for point in points:
		new_points.append((int(point[0]/int(width)*CONST), int(point[1]/int(height)*CONST)))
	return new_points

def capturer(n):
	points = []
	root = tk.Tk()
	root.attributes("-fullscreen", True)
	label = tk.Label(root, text=str(n), font = "Arial 90", bg="deep sky blue", fg="white")
	label.pack(side="top", expand="yes", fill="both")
	label.bind("<Button-1>", lambda event: handler(event, points, n))
	root.mainloop()
	points = resizer(points, root.geometry().split("+")[0])
	root.destroy()
	return points
	
if __name__ == "__main__":
	"""
	#Первый тест!
	points = capturer(5)
	print("Калибровка на 5 точках с отключенными большими касаниями")
	path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "a.exe")
	proc = subprocess.Popen((path, "1", "2", "0", "3", "12"), stdin=subprocess.PIPE, stdout=subprocess.PIPE)
	
	arguments = ""
	for point in points:
		arguments += str(point[0]) + " " + str(point[1]) + " "
	arguments += "\r\n"
		
	print("Sending points:", arguments)
	proc.communicate(input=bytes(arguments, encoding="ascii"))
	print(proc.communicate()[0].decode("ascii"))

	#Второй тест! 
	points = capturer(9)
	print("Калибровка на 9 точках с отключенными большими касаниями")
	path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "a.exe")
	proc = subprocess.Popen((path, "1", "2", "11", "3", "12"), stdin=subprocess.PIPE, stdout=subprocess.PIPE)
	
	arguments = ""
	for point in points:
		arguments += str(point[0]) + " " + str(point[1]) + " "
	arguments += "\r\n"
		
	print("Sending points:", arguments)
	proc.communicate(input=bytes(arguments, encoding="ascii"))
	print(proc.communicate()[0].decode("ascii"))
	
	#Третий тест!
	print("Правильная калибровка на 5 точках с включенными большими касаниями")
	arguments = "5000 5000 20000 5000 20000 20000 5000 20000 10000 10000"
	print("Sending points:", arguments)
	path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "a.exe")
	proc = subprocess.Popen((path, "1", "2", "0", "3", "12", "13"), stdin=subprocess.PIPE, stdout=subprocess.PIPE)
	proc.communicate(input=bytes(arguments, encoding="ascii"))
	print(proc.communicate()[0].decode("ascii"))
	
	#Четвёртый тест!
	print("Определение устройства, старт/стоп, включить/выключить устройство, большие касания")
	path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "a.exe")
	proc = subprocess.Popen((path, "1", "2", "3", "6", "8", "9", "12", "13"), stdin=subprocess.PIPE, stdout=subprocess.PIPE)
	print(proc.communicate()[0].decode("ascii"))
	"""
	path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "a.exe")
	proc = subprocess.Popen((path,"13" , "6"), stdin=subprocess.PIPE, stdout=subprocess.PIPE)
	print(proc.communicate()[0].decode("ascii"))
	
