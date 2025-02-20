#include <gmock/gmock.h>

#include <thread>
#include <array>
#include <list>
#include <iostream>
#include <sstream>

#include <RingBuffer.h>

using namespace glbinding;

class RingBuffer_test : public testing::Test
{
public:
};

TEST_F(RingBuffer_test, SimpleTest)
{
    RingBuffer<int> buffer(10);
    EXPECT_EQ(10u, buffer.maxSize());
    EXPECT_EQ(true, buffer.isEmpty());

    RingBuffer<int>::TailIdentifier tail = buffer.addTail();
    auto it = buffer.cbegin(tail);
    EXPECT_FALSE(buffer.valid(tail, it));
    EXPECT_EQ(0u, buffer.size(tail));
    EXPECT_EQ(0u, buffer.size());

    for (int i = 0; i < 10; i++)
    {
        EXPECT_EQ(true, buffer.push(i));
    }

    EXPECT_EQ(10u, buffer.size());
    EXPECT_EQ(10u, buffer.size(tail));
    EXPECT_FALSE(buffer.push(11));
    EXPECT_TRUE(buffer.isFull());

    EXPECT_TRUE(buffer.valid(tail, it));

    for (int i = 0; i < 5; i++)
    {
        EXPECT_EQ(true, buffer.valid(tail, it));
       int j = *it;
       EXPECT_EQ(i, j);
        it = buffer.next(tail, it);
        EXPECT_EQ(buffer.size(tail), buffer.size());
    }

    EXPECT_EQ(5u, buffer.size());

    for (int i = 10; i < 15; i++)
    {
        EXPECT_EQ(true, buffer.push(i));
    }

    EXPECT_EQ(10u, buffer.size());

    for (int i = 5; i < 15; i++)
    {
        EXPECT_EQ(true, buffer.valid(tail, it));
        EXPECT_EQ(i, *it);
        it = buffer.next(tail, it);
        EXPECT_EQ(buffer.size(tail), buffer.size());
    }

    for (int i = 0; i < 7; i++)
    {
        EXPECT_EQ(true, buffer.push(i));
    }

    for (int i = 0; i < 5; i++)
    {
        EXPECT_EQ(true, buffer.valid(tail, it));
        EXPECT_EQ(i, *it);
        it = buffer.next(tail, it);
        EXPECT_EQ(buffer.size(tail), buffer.size());
    }

    EXPECT_EQ(2u, buffer.size());

    buffer.removeTail(tail);
    EXPECT_EQ(0u, buffer.size());
}

TEST_F(RingBuffer_test, StringTest)
{
    RingBuffer<std::string> buffer(10);
    EXPECT_EQ(10u, buffer.maxSize());
    EXPECT_TRUE(buffer.isEmpty());

    RingBuffer<int>::TailIdentifier tail = buffer.addTail();

    for (int i = 0; i < 10; i++)
    {
        std::ostringstream oss;
        oss << i;
        EXPECT_EQ(true, buffer.push("Hello world " + oss.str()));
    }

    auto it = buffer.cbegin(tail);

    for (int i = 0; i < 10; i++)
    {
        EXPECT_EQ(true, buffer.valid(tail, it));
        std::ostringstream oss;
        oss << i;
        std::string expected = "Hello world " + oss.str();
        EXPECT_EQ(expected, *it);
        it = buffer.next(tail, it);
        EXPECT_EQ(buffer.size(tail), buffer.size());
    }

    EXPECT_EQ(0u, buffer.size());
    EXPECT_FALSE(buffer.valid(tail, it));
}

// Test behavior with objects
struct MockObject {
    int value;
    int* pointer;
};

TEST_F(RingBuffer_test, ObjectTest)
{
    RingBuffer<MockObject> buffer(10);
    EXPECT_EQ(10u, buffer.maxSize());
    EXPECT_TRUE(buffer.isEmpty());

    int i0 = 0;
    MockObject obj0 = {0, &i0};
    int i1 = 1;
    MockObject obj1 = {1, &i1};
    int i2 = 2;
    MockObject obj2 = {2, &i2};
    int i3 = 3;
    MockObject obj3 = {3, &i3};
    int i4 = 4;
    MockObject obj4 = {4, &i4};

    RingBuffer<int>::TailIdentifier tail = buffer.addTail();


    buffer.push(obj0);
    buffer.push(obj1);
    buffer.push(obj2);
    buffer.push(obj3);
    buffer.push(obj4);

    auto it = buffer.cbegin(tail);

    for (int i = 0; i < 5; i++)
    {
        EXPECT_EQ(true, buffer.valid(tail, it));
        EXPECT_EQ(i, (*it).value);
        EXPECT_EQ(i, *(*it).pointer);
        it = buffer.next(tail, it);
        EXPECT_EQ(buffer.size(tail), buffer.size());
    }

    EXPECT_EQ(0u, buffer.size());
    EXPECT_FALSE(buffer.valid(tail, it));
}

TEST_F(RingBuffer_test, ObjectPointerTest)
{
    RingBuffer<MockObject*> buffer(10);
    EXPECT_EQ(10u, buffer.maxSize());
    EXPECT_TRUE(buffer.isEmpty());

    int i0 = 0;
    MockObject obj0 = {0, &i0};
    int i1 = 1;
    MockObject obj1 = {1, &i1};
    int i2 = 2;
    MockObject obj2 = {2, &i2};
    int i3 = 3;
    MockObject obj3 = {3, &i3};
    int i4 = 4;
    MockObject obj4 = {4, &i4};

    RingBuffer<int>::TailIdentifier tail = buffer.addTail();


    buffer.push(&obj0);
    buffer.push(&obj1);
    buffer.push(&obj2);
    buffer.push(&obj3);
    buffer.push(&obj4);

    auto it = buffer.cbegin(tail);

    for (int i = 0; i < 5; i++)
    {
        EXPECT_EQ(true, buffer.valid(tail, it));
        EXPECT_EQ(i, (*it)->value);
        EXPECT_EQ(i, *(*it)->pointer);
        it = buffer.next(tail, it);
        EXPECT_EQ(buffer.size(tail), buffer.size());
    }

    EXPECT_EQ(0u, buffer.size());
    EXPECT_FALSE(buffer.valid(tail, it));
}

TEST_F(RingBuffer_test, SimpleMultiThreadedTest)
{

    RingBuffer<int> buffer(1000);
    int testSize = 100000;
    RingBuffer<int>::TailIdentifier tail = buffer.addTail();

    std::thread t1([&]()
    {
        for(int i = 0; i < testSize; i++)
            while(!buffer.push(i));
    });

    std::thread t2([&]()
    {
        auto it = buffer.cbegin(tail);

        int i = 0;
        while (i < testSize)
        {
            if (buffer.valid(tail, it))
            {
                EXPECT_EQ(i++, *it);
                it = buffer.next(tail, it);
            };

        }

        EXPECT_EQ(0u, buffer.size(tail));
    });

    t1.join();
    t2.join();
    EXPECT_EQ(0u, buffer.size());
}

TEST_F(RingBuffer_test, MultiThreadedTest2)
{

    RingBuffer<int> buffer(1000);
    int testSize = 100000;
    RingBuffer<int>::TailIdentifier tail1 = buffer.addTail();
    RingBuffer<int>::TailIdentifier tail2 = buffer.addTail();

    std::thread t1([&]()
    {
        for(int i = 0; i < testSize; i++)
            while(!buffer.push(i));
    });

    std::thread t2([&]()
    {
        auto it = buffer.cbegin(tail1);

        int i = 0;
        while (i < testSize)
        {
            if (buffer.valid(tail1, it))
            {
                EXPECT_EQ(i++, *it);
                it = buffer.next(tail1, it);
            };

        }

        EXPECT_EQ(0u, buffer.size(tail1));
    });

    std::thread t3([&]()
    {
        auto it = buffer.cbegin(tail2);

        int i = 0;
        while (i < testSize)
        {
            if (buffer.valid(tail2, it))
            {
                EXPECT_EQ(i++, *it);
                it = buffer.next(tail2, it);
            };

        }

        EXPECT_EQ(0u, buffer.size(tail2));
    });

    t1.join();
    t2.join();
    t3.join();
    EXPECT_EQ(0u, buffer.size());
}

TEST_F(RingBuffer_test, ResizeTest)
{

    RingBuffer<int> buffer(10);

    EXPECT_EQ(0u, buffer.size());
    EXPECT_EQ(10u, buffer.maxSize());

    buffer.resize(20u);

    EXPECT_EQ(0u, buffer.size());
    EXPECT_EQ(20u, buffer.maxSize());
}


