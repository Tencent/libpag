
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

glGenVertexArrays(1, &vao);
glGenBuffers(1, &quad);

program = glCreateProgram();

vs = glCreateShader(GL_VERTEX_SHADER);
glShaderSource(vs, 1, &vert, 0);
glCompileShader(vs);

fs = glCreateShader(GL_FRAGMENT_SHADER);
glShaderSource(fs, 1, &frag, 0);
glCompileShader(fs);

glAttachShader(program, vs);
glAttachShader(program, fs);
glLinkProgram(program);

glBindBuffer(GL_ARRAY_BUFFER, quad);
glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * 4, vertices, GL_STATIC_DRAW);

glBindVertexArray(vao);

a_vertex = static_cast<GLuint>(glGetAttribLocation(program, "a_vertex"));
glEnableVertexAttribArray(static_cast<GLuint>(a_vertex));
glVertexAttribPointer(static_cast<GLuint>(a_vertex), 2, GL_FLOAT, GL_FALSE, 0, nullptr);

glUseProgram(program);

glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

glDeleteProgram(program);
glDeleteBuffers(1, &quad);
glDeleteVertexArrays(1, &vao);
