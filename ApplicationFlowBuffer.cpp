#include "ApplicationFlowBuffer.h"
const char CR = '\r';
const char LF = '\n';
const int FLOW_BUFF_MAX = 65536;
ApplicationFlowBuffer::ApplicationFlowBuffer(/* args */)
{
    buffer_length_ = 0;
    buffer_ = nullptr;
    orig_data_begin_ = nullptr;
    orig_data_end_ = nullptr;
    ResetLineState();
    mode_ = UNKNOWN_MODE;
    frame_length_ = 0;
    chunked_ = false;
    data_seq_at_orig_data_end = 0;
    eof_ = false;
    have_pending_request_ = false;
    buffer_n_ = 0;
    NewMessage();
}
ApplicationFlowBuffer::ApplicationFlowBuffer(LineBreakStyle line_style)
{
    ApplicationFlowBuffer();
    linebreak_style_ = line_style;
}

ApplicationFlowBuffer::~ApplicationFlowBuffer()
{
    if (buffer_)
    {
        free(buffer_);
    }
}
void ApplicationFlowBuffer::NewMessage()
{
    assert(frame_length_ >= 0);
    int byte_to_advance = 0;
    if (buffer_n_ == 0)
    {
        switch (mode_)
        {
        case LINE_MODE:
            /* code */
            byte_to_advance = (frame_length_ + (linebreak_style_ == STRICT_CRLF ? 2 : 1));
            break;
        case FRAME_MODE:
            /* code */
            byte_to_advance = frame_length_;
            break;
        case UNKNOWN_MODE:
            /* code */
            break;
        default:
            break;
        }
        orig_data_begin_ += byte_to_advance;
        assert(orig_data_begin_ <= orig_data_end_);
        buffer_n_ = 0;
        message_complete_ = false;
    }
}
void ApplicationFlowBuffer::ResetLineState()
{
    switch (linebreak_style_)
    {
    case CR_OR_LF:
        /* code */
        state_ = CR_OR_LF_0;
        break;
    case STRICT_CRLF:
        /* code */
        state_ = STRICT_CRLF_0;
        break;
    default:
        assert(0);
        break;
    }
}
bool ApplicationFlowBuffer::ExpandBuffer(int length)
{
    bool ret = true;
    if (length >= FLOW_BUFF_MAX)
    {
        ret = false;
    }
    else
    {
        if (length < 512)
        {
            length = 512;
        }
        if (length < buffer_length_ * 2)
        {
            length = buffer_length_ * 2;
        }
        unsigned char *new_buf = (unsigned char *)realloc(buffer_, length);
        if (new_buf == nullptr)
        {
            ret = false;
        }
        else
        {
            //memory realloc successfully
            buffer_length_ = length;
            buffer_ = new_buf;
        }
    }
    return ret;
}
void ApplicationFlowBuffer::NewLine()
{
    NewMessage();
    mode_ = LINE_MODE;
    frame_length_ = 0;
    chunked_ = false;
    have_pending_request_ = true;
    MarkOrCopyFrame();
}
void ApplicationFlowBuffer::NewFrame(int frame_length, bool chunked)
{
    NewMessage();
    mode_ = FRAME_MODE;
    frame_length_ = frame_length;
    chunked_ = chunked;
    have_pending_request_ = true;
    MarkOrCopyFrame();
}
void ApplicationFlowBuffer::BufferData(BytePoint data, BytePoint end)
{
    if (data < end)
    {
        mode_ = FRAME_MODE;
        frame_length_ += end - data;
        MarkOrCopyFrame();
        NewData(data, end);
    }
}
void ApplicationFlowBuffer::FinishBuffer()
{
    message_complete_ = true;
}
void ApplicationFlowBuffer::GrowFrame(int length)
{
    assert(frame_length_ >= 0);
    if (length <= frame_length_)
    {
        return;
    }
    assert(!chunked_ || frame_length_ == 0);
    mode_ = FRAME_MODE;
    frame_length_ = length;
    MarkOrCopyFrame();
}
void ApplicationFlowBuffer::DiscardData()
{
    mode_ = UNKNOWN_MODE;
    message_complete_ = false;
    have_pending_request_ = false;
    orig_data_begin_ = orig_data_end_ = 0;
    buffer_n_ = 0;
    frame_length_ = 0;
}
void ApplicationFlowBuffer::set_eof()
{
    eof_ = true;
    if (chunked_)
    {
        frame_length_ = orig_data_end_ - orig_data_begin_;
    }
    if (frame_length_ < 0)
    {
        frame_length_ = 0;
    }
}
void ApplicationFlowBuffer::NewData(BytePoint begin, BytePoint end)
{
    assert(begin < end);
    ClearPreviousData();
    assert((buffer_n_ == 0 && message_complete_) || orig_data_begin_ == orig_data_end_);
    orig_data_begin_ = begin;
    orig_data_end_ = end;
    data_seq_at_orig_data_end += (end - begin);
    MarkOrCopy();
}
void ApplicationFlowBuffer::NewData(const string &data)
{
    BytePoint begin = (BytePoint)(data.c_str());
    BytePoint end = begin + data.size();
    NewData(begin, end);
}
void ApplicationFlowBuffer::MarkOrCopy()
{
    if (!message_complete_)
    {
        switch (mode_)
        {
        case LINE_MODE:
            /* code */
            MarkOrCopyLine();
            break;
        case FRAME_MODE:
            /* code */
            MarkOrCopyFrame();
            break;
        default:
            break;
        }
    }
}
void ApplicationFlowBuffer::ClearPreviousData()
{
    //previous data must have been processed or buffered already
    if (orig_data_begin_ < orig_data_end_)
    {
        assert(buffer_n_ == 0);
        if (chunked_)
        {
            if (frame_length_ > 0)
            {
                frame_length_ -= (orig_data_end_ - orig_data_begin_);
            }
            orig_data_begin_ = orig_data_end_;
        }
    }
}

void ApplicationFlowBuffer::NewGap(int length)
{
    ClearPreviousData();
    if (chunked_ && frame_length_ >= 0)
    {
        frame_length_ -= length;
        if (frame_length_ < 0)
        {
            frame_length_ = 0;
        }
    }
    orig_data_begin_ = orig_data_end_ = 0;
    MarkOrCopy();
}

void ApplicationFlowBuffer::MarkOrCopyLine()
{
    switch (linebreak_style_)
    {
    case CR_OR_LF:
        MarkOrCopyLine_CR_OR_LF();
        break;
    case STRICT_CRLF:
        MarkOrCopyLine_STRICT_CRLF();
        break;
    default:
        break;
    }
}

void ApplicationFlowBuffer::MarkOrCopyLine_CR_OR_LF()
{
    if (!(orig_data_begin_ && orig_data_end_))
    {
        return;
    }
    if (state_ == CR_OR_LF_1 && orig_data_begin_ < orig_data_end_ && *orig_data_begin_ == LF)
    {
        state_ = CR_OR_LF_0;
        ++orig_data_begin_;
    }
    BytePoint data;
    for (data = orig_data_begin_; data < orig_data_end_; ++data)
    {
        switch (*data)
        {
        case CR:
            state_ = CR_OR_LF_1;
            goto found_end_of_line;
        case LF:
            goto found_end_of_line;

        default:
            break;
        }
    }
    AppendToBuffer(orig_data_begin_, orig_data_end_ - orig_data_begin_);
    return;
found_end_of_line:
    if (buffer_n_ == 0)
    {
        frame_length_ = data - orig_data_begin_;
    }
    else
    {
        AppendToBuffer(orig_data_begin_, data + 1 - orig_data_begin_);
        --buffer_n_;
    }
    message_complete_ = true;
}

void ApplicationFlowBuffer::MarkOrCopyLine_STRICT_CRLF()
{
    BytePoint data;
    if (!(orig_data_begin_ && orig_data_end_))
    {
        return;
    }
    for (data = orig_data_begin_; data < orig_data_end_; ++data)
    {
        switch (*data)
        {
        case CR:
            state_ = STRICT_CRLF_1;
            break;
        case LF:
            if (state_ == STRICT_CRLF_1)
            {
                state_ == STRICT_CRLF_0;
                goto found_end_of_line;
            }
            break;
        default:
            break;
        }
    }
    AppendToBuffer(orig_data_begin_, orig_data_end_ - orig_data_begin_);
    return;
found_end_of_line:
    if (buffer_n_ == 0)
    {
        frame_length_ = data - 1 - orig_data_begin_;
    }
    else
    {
        AppendToBuffer(orig_data_begin_, data + 1 - orig_data_begin_);
        buffer_n_ -= 2;
    }
    message_complete_ = true;
}

void ApplicationFlowBuffer::MarkOrCopyFrame()
{
    if (!(orig_data_begin_ && orig_data_end_))
    {
        return;
    }
    if (mode_ == FRAME_MODE && state_ == CR_OR_LF_1 && orig_data_begin_ < orig_data_end_)
    {
        if (*orig_data_begin_ == LF)
        {
            ++orig_data_begin_;
        }
        state_ = FRAME_0;
    }
    if (buffer_n_ == 0)
    {
        if (frame_length_ >= 0 && orig_data_begin_ < orig_data_end_ && !chunked_)
        {
            message_complete_ = true;
        }
        else
        {
            AppendToBuffer(orig_data_begin_, orig_data_end_ - orig_data_begin_);
            message_complete_ = false;
        }
    }
    else
    {
        int byte_to_copy = orig_data_end_ - orig_data_begin_;
        message_complete_ = false;
        if (frame_length_ >= 0 && buffer_n_ + byte_to_copy >= frame_length_)
        {
            byte_to_copy = frame_length_ - buffer_n_;
            message_complete_ = true;
        }
        AppendToBuffer(orig_data_begin_, byte_to_copy);
    }
}

void ApplicationFlowBuffer::AppendToBuffer(BytePoint data, int len)
{
    if (len <= 0)
    {
        return;
    }
    if (ExpandBuffer(buffer_n_ + len))
    {
        memcpy(buffer_ + buffer_n_, data, len);
        buffer_n_ += len;
        orig_data_begin_ += len;
    }
    else
    {
    }
    assert(orig_data_begin_ < orig_data_end_);
}