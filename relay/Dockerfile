FROM python:3.13-alpine
RUN adduser -D relay
USER relay
COPY server.py /server.py
EXPOSE 8787
CMD ["python3", "-u", "/server.py"]
