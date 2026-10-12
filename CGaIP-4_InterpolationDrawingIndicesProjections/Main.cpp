#include <gl/glew.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <random>
#include <iostream>

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> distrib(1, 4);
int randomNumber = 1;

const GLint WIDTH = 800, HEIGHT = 800; // Константи початкової ширини та висоти вікна
GLuint shader, uniformModel, uniformProjection; // Глобальні змінні для об'єктів у пам'яті відеокарти shader - ID скомпільованої шейдерної програми, а uniformModel - ID для передачі матриці 4х4 у вершинний шейдер...
const float toRadians = 3.14159265f / 180.0f; // Коефіцієнт для переведення кутів із градусів у радіани
float curAngle = 0.0f; // Поточний кут повороту фігури
// Структура, що об'єднує дані одного об'єкта
struct Figure {
	GLuint VAO; // ID масиву налаштувань буферів
	GLuint VBO; // ID буфера з чистими координатами у відеопам'яті
	GLuint IBO;
	int verteciesAmount; // Реальна кількість вершин у фігурі
};
bool direction = true;
float triOffset = 0.0f;
float triMaxOffset = 0.7f;
float triIncrement = 0.0002f;
bool sizeDirection = true;
float curSize = 0.4f;
float maxSize = 0.8f;
float minSize = 0.1f;

// Створюємо текст вершиного шейдера, який використовується для перетворення вершин у відповідні координати на екрані
static const char* vShader = R"(
	#version 330 // Вказуємо відеокарті, що код написаний на мові GLSL версії 3.3
	// in (це вхідні дані, які приходять у шейдер ззовні)
	// vec3 (координати у форматі вектора 3-х чисел типу float для кожної точки)
	// layout (location = 0) номер порту, який зв'язує C++ масив із цим шейдером
	layout(location = 0) in vec3 pos;
	out vec4 vCol;  
	uniform mat4 model; // Створюємо Uniform-змінну для передачі матриці моделі 4х4
	uniform mat4 projection;
	void main()	{ // Головна функція шейдера, яка викликається окремо для кожної вершини
			gl_Position = projection * model * vec4(pos, 1.0); // Змінна, в яку записується фінальна позиція точки на екрані, вона вимагає 3 координати (X, Y, Z) та 4 - перспективу 
			vCol = vec4(clamp(pos, 0.0f, 1.0f), 1.0);
	}
)";

// Створюємо текст фрагметного шейдера, який буде керувати кольором пікселей
static const char* fShader = R"(
	#version 330
	in vec4 vCol;
	out vec4 colour; // out (вихідні дані, які шейдер відає назовні для подальшого відображення пікселю на екран
		void main()	{ // Головна функція шейдера, яка викликається окремо для кожного пікселя
			colour = vCol; // Керуємо коліром в форматі RGBA, присвоюємо пікселю той колір, який передасть C++
		}
)";

// Функція створення фігури
Figure CreateFigure(GLfloat* vertices, int size, unsigned short* indices, int sizeIndices) {
	Figure newFig;
	newFig.verteciesAmount = size / (3 * sizeof(GLfloat)); // Рахуємо кількість вершин
	glGenVertexArrays(1, &newFig.VAO); // Створюємо VAO, яка буде запам'ятовувати всі налаштування буферів 
	glBindVertexArray(newFig.VAO); // Активуємо VAO

	glGenBuffers(1, &newFig.IBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, newFig.IBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeIndices, indices, GL_STATIC_DRAW);

	glGenBuffers(1, &newFig.VBO); // Створюємо VBO буфер, куди фізично завантажимо координати фігури
	glBindBuffer(GL_ARRAY_BUFFER, newFig.VBO); // Активуємо VBO
	glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW); // Копіюємо масив точок з ОЗП у VRAM
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0); // Створюємо декодер-пояснення як читати байти (слот №0, 3 числа, типу флоат, без нормалізації, крок 0, зміщення 0)
	glEnableVertexAttribArray(0); // Вмикаємо декодер для слоту 0
	glBindBuffer(GL_ARRAY_BUFFER, 0); // Відв'язуємо буфери, щоб випадково не стерти дані в іншому місці програми
	glBindVertexArray(0);
	return newFig;
}

// Допоміжна функція для компіляції одного шейдера
void AddShader(GLuint theProgram, const char* shaderCode, GLenum shaderType)
{
	GLuint theShader = glCreateShader(shaderType); // Створюємо порожній об'єкт шейдера потрібного типу (вершинний або фрагментний)
	const GLchar* theCode[1];
	theCode[0] = shaderCode; // Передаємо текст програми нашого шейдера
	GLint codeLength[1];
	codeLength[0] = strlen(shaderCode); // Рахуємо довжину тексту
	glShaderSource(theShader, 1, theCode, codeLength); // Передаємо текст шейдера у створений порожній об'єкт шейдера
	glCompileShader(theShader); // Компілюємо шейдер
	// Перевіряємо на помилки в коді GLSL шейдера
	GLint result = 0;
	GLchar eLog[1024] = { 0 };
	glGetShaderiv(theShader, GL_COMPILE_STATUS, &result);
	if (!result)
	{
		glGetShaderInfoLog(theShader, 1024, NULL, eLog);
		fprintf(stderr, "Error compiling the %d shader:	'%s'\n", shaderType, eLog); // Якщо компіляція провалилася до виводимо інформацію
		return;
	}
	glAttachShader(theProgram, theShader); // Інакше прикріплюємо цей скомпільований шейдер до нашої програми
}

void CompileShaders() {
	shader = glCreateProgram(); // Створюємо програму-інструкцію для відеокарти
	if (!shader)
	{
		printf("Failed to create shader\n");
		return;
	}
	AddShader(shader, vShader, GL_VERTEX_SHADER); // Додаємо та компілюємо вершинний шейдер
	AddShader(shader, fShader, GL_FRAGMENT_SHADER); // Додаємо та компілюємо фрагментний шейдер
	GLint result = 0;
	GLchar eLog[1024] = { 0 };
	glLinkProgram(shader); // З'єднує шейдер в одну інструкцію
	// Перевіряємо чи успішно вони з'єдналися між собою
	glGetProgramiv(shader, GL_LINK_STATUS, &result);
	if (!result)
	{
		glGetProgramInfoLog(shader, sizeof(eLog), NULL, eLog);
		printf("Error linking program: '%s'\n", eLog);
		return;
	}
	// Перевіряємо, чи зможе взагалі цей шейдер запуститися на поточній відеокарті
	glValidateProgram(shader);
	glGetProgramiv(shader, GL_VALIDATE_STATUS, &result);
	if (!result)
	{
		glGetProgramInfoLog(shader, sizeof(eLog), NULL, eLog);
		printf("Error validating program: '%s'\n", eLog);
		return;
	}
	uniformModel = glGetUniformLocation(shader, "model"); // находимо адресу змінної "model" всередині шейдеру і записуємо її в глобальний ідентифікатор uniformModel
	uniformProjection = glGetUniformLocation(shader, "projection");
}

int main() {
	if (!glfwInit()) // Спроба ініціалізації бібліотеки GLFW, яка потрібна для створення віконих інтерфейсів та обробки подій в графічних програмах
	{
		printf("GLFW failed to start :(");
		glfwTerminate(); // У разі не ініціалізації бібліотеки GLFW виконується очищення ресурсів і завершення виконання програми
		return 1;
	}
	// Налаштовуємо OpenGL перед створенням вікна
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // Встановлюємо головну версію OpenGL 
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); // Встановлюємо додаткову версію OpenGL 
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // Використовуємо Core-профіль, профіль сучасного OpenGL без застарілих функцій
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Вмикаємо зворотну сумісність для коректної роботи з macOS та новими драйверами
	GLFWwindow* mainWindow = glfwCreateWindow(WIDTH, HEIGHT, "by Vladislav Samoilov KNT-223", NULL, NULL); // Створюємо головне вікно програми із заданими розмірами та заколовком
	if (!mainWindow) // Перевіряємо ініціалізацію вікна
	{
		printf("GLFW window creation failed :(");
		glfwTerminate(); // Закриваємо GLFW, якщо вікно не вдалося створити та завершаємо виконання програми
		return 1;
	}
	int bufferWidth, bufferHeight; // Змінні для збереження реального розміру області рендерингу (в пікселях)
	glfwGetFramebufferSize(mainWindow, &bufferWidth, &bufferHeight); // Отримуємо фактичний розмір буфера кадру вікна
	glfwMakeContextCurrent(mainWindow); // Робимо контекст OpenGL створеного вікна поточним для цього потоку програми
	glewExperimental = GL_TRUE; // Дозволяємо GLEW завантажувати експериментальні та нові розширення OpenGL
	if (glewInit() != GLEW_OK) // Перевіряємо ініціалізацію бібліотеки GLEW, який динамічно завантажує функції OpenGL для відеокарти
	{
		printf("GLEW failed to start >_<");
		glfwDestroyWindow(mainWindow); // Знищуємо створене вікно
		glfwTerminate(); // Завершуємо роботу GLFW
		return 1;
	}
	glEnable(GL_DEPTH_TEST);
	glViewport(0, 0, bufferWidth, bufferHeight); // Налаштовуємо область перегляду OpenGL
	// Координати вершин куба
	GLfloat squareV[] = {
		-1.0f, 1.0f, 1.0f,
		-1.0f, -1.0f, 1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f, 1.0f, -1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, -1.0f, 1.0f,
		1.0f, -1.0f, -1.0f,
		1.0f, 1.0f, -1.0f
	};
	unsigned short indices[] = {
		0, 1, 2,
		0, 2, 3,
		0, 1, 5,
		0, 4, 5,
		4, 5, 6,
		4, 6, 7,
		0, 4, 7,
		0, 3, 7,
		1, 2, 6,
		1, 5, 6,
		2, 3, 6,
		3, 6, 7
	};

	Figure squre = CreateFigure(squareV, sizeof(squareV), indices, sizeof(indices)); // Створюємо геометрію куба
	CompileShaders(); // Компілюємо шейдери
	glm::mat4 projection = glm::perspective(glm::radians(90.0f), (GLfloat)bufferWidth / (GLfloat)bufferHeight, 0.1f, 100.0f);
	glm::mat4 projectionORTHO = glm::ortho(0.0f, (GLfloat)bufferWidth, 0.0f, (GLfloat)bufferHeight, -100.0f, 100.0f);
	while (!glfwWindowShouldClose(mainWindow)) // Головний цикл роботи програми, працює, поки користувач не закриє вікно
	{
		glfwPollEvents(); // Перевіряємо та обробляємо події
		
		curAngle += 1;
		if (curAngle >= 360) {
			curAngle -= 360;
		}
		float time = (float)glfwGetTime(); // Отримуємо точний час роботи програми в секундах від GLFW
		float moveImpulse = powf(sinf(time), 6.0f) * 20; 
		
		glClearColor(0.0f, 0.3f, 1.0f, 1.0f); // Встановлюємо колір для очищення екрану RGBA
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Очищаємо буфер кольору кадру за допомогою вказаного вище кольору
		glUseProgram(shader); // Вмикаємо шейдерну програму
		glm::mat4 model(1.0f); // Ініціалізація одиничної матриці моделі
		model = glm::translate(model, glm::vec3(0.0f, 1.0f, -3.0f - moveImpulse)); // Додаємо зміщення по осях Х та У
		model = glm::rotate(model, 45.0f * toRadians, glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::rotate(model, curAngle * toRadians, glm::vec3(0.0f, 1.0f, 0.0f)); // Додаємо обертання об'єкта навколо осі Z
		model = glm::scale(model, glm::vec3(0.5f, 0.5f, 0.5f)); // Додаємо масштаб, множимо осі X та Y на коефіцієнт curSize
		glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model)); // Передаємо готову фінальну матрицю 4х4 у вершинний шейдер без перевороту (GL_FALSE)
		glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projection));
		
		
		glBindVertexArray(squre.VAO); // Вмикаємо налаштування фігури
		glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 0); // Даємо команду намалювати фігуру прямокутник

		glm::mat4 modelORTHO(1.0f); // Ініціалізація одиничної матриці моделі
		modelORTHO = glm::translate(modelORTHO, glm::vec3((float)bufferWidth / 2.0f, 200.0f, -40.0f - moveImpulse)); // Додаємо зміщення по осях Х та У
		modelORTHO = glm::rotate(modelORTHO, 30.0f * toRadians, glm::vec3(1.0f, 0.0f, 0.0f));
		modelORTHO = glm::rotate(modelORTHO, curAngle * toRadians, glm::vec3(0.0f, 1.0f, 0.0f)); // Додаємо обертання об'єкта навколо осі Z
		modelORTHO = glm::scale(modelORTHO, glm::vec3(80.0f, 80.0f, 80.0f)); // Додаємо масштаб, множимо осі X та Y на коефіцієнт curSize
		glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(modelORTHO)); // Передаємо готову фінальну матрицю 4х4 у вершинний шейдер без перевороту (GL_FALSE)

		glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projectionORTHO));
		glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 0); // Даємо команду намалювати фігуру прямокутник

		// Очищення, для того, щоб інші об'єкти не малювалися цим шейдером
		glBindVertexArray(0);
		glUseProgram(0);
		glfwSwapBuffers(mainWindow); // Міняємо місцями задній буфер (де ми малювали) і передній буфер (який бачить користувач)
	}
	return 0;
}