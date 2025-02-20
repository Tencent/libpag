
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
glError();

glGenVertexArrays(1, &vao);
glError();

glGenBuffers(1, &quad);
glError();

program = glCreateProgram();
glError();

vs = glCreateShader(GL_VERTEX_SHADER);
glError();
glShaderSource(vs, 1, &vert, 0);
glError();
glCompileShader(vs);
glError();

fs = glCreateShader(GL_FRAGMENT_SHADER);
glError();
glShaderSource(fs, 1, &frag, 0);
glError();
glCompileShader(fs);
glError();

glAttachShader(program, vs);
glError();
glAttachShader(program, fs);
glError();
glLinkProgram(program);
glError();

glBindBuffer(GL_ARRAY_BUFFER, quad);
glError();
glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * 4, vertices, GL_STATIC_DRAW);
glError();

glBindVertexArray(vao);
glError();

a_vertex = static_cast<GLuint>(glGetAttribLocation(program, "a_vertex"));
glError();
glEnableVertexAttribArray(static_cast<GLuint>(a_vertex));
glError();
glVertexAttribPointer(static_cast<GLuint>(a_vertex), 2, GL_FLOAT, GL_FALSE, 0, nullptr);
glError();

glUseProgram(program);
glError();

glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
glError();

glDeleteProgram(program);
glError();
glDeleteBuffers(1, &quad);
glError();
glDeleteVertexArrays(1, &vao);
glError();
