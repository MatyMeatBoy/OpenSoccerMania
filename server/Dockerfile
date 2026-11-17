FROM python:3.11-slim
WORKDIR /app
COPY opensoccermania /app/opensoccermania
RUN useradd --system --uid 10001 soccer
USER 10001
EXPOSE 27845/tcp
CMD ["python", "-m", "opensoccermania.netplay.server", "--bind", "0.0.0.0", "--port", "27845"]
